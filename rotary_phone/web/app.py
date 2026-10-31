"""Mobile web remote: each button publishes one MQTT message to the phone; the page polls the phone's status."""

import logging
import time
from dataclasses import dataclass

import paho.mqtt.publish as mqtt_publish
from flask import Flask
from flask import abort
from flask import jsonify
from flask import render_template
from flask import request

from rotary_phone.web.config import FLASK_PORT
from rotary_phone.web.config import MQTT_HOST
from rotary_phone.web.config import MQTT_PORT
from rotary_phone.web.config import MQTT_TOPIC
from rotary_phone.web.phone_events import EventLog
from rotary_phone.web.phone_events import PhoneState
from rotary_phone.web.phone_events import describe
from rotary_phone.web.phone_events import start_listener

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

app = Flask(__name__)


@dataclass(frozen=True)
class Action:
    id: str
    label: str
    payload: str
    track: int | None = None
    icon: str = "phone-call"


@dataclass(frozen=True)
class Section:
    title: str
    actions: list[Action]


def _track(number: int, label: str) -> Action:
    return Action(f"track-{number}", label, str(number), number)


def _sound(number: int, label: str) -> Action:
    return Action(f"sound-{number}", label, f"play:{number}", number, "speaker-high")


SECTIONS = [
    Section(
        "Songs",
        [
            _track(30, "Rick Roll"),
            _track(31, "IT Crowd: 0118 999 881 999 119 7253"),
            _track(32, "Jenny, 867-5309"),
            _track(33, "Taken phone speech"),
        ],
    ),
    Section(
        "Messages",
        [
            _track(0, "The wireless customer you are calling"),
            _track(1, "Welcome to Verizon, we're sorry"),
            _track(2, "Unfortunately the system cannot process"),
            _track(3, "Hello, please listen carefully"),
            _track(4, "Welcome to Verizon Wireless while roaming"),
            _track(5, "The service you are attempting to use"),
            _track(6, "If this is an emergency"),
            _track(7, "Sorry, your account has no access"),
            _track(8, "The magicJack customer"),
            _track(9, "The person you're trying to reach"),
            _track(10, "Your account has a zero balance"),
            _track(11, "Just for calling in today"),
            _track(99, "Help, I'm stuck"),
        ],
    ),
    Section(
        "Sounds",
        [
            _sound(12, "Beep beep beep"),
            _sound(13, "Fax machine"),
            _sound(14, "R2-D2"),
            _sound(98, "Windows XP startup"),
        ],
    ),
]
STOP = Action("stop", "Stop", "stop")
BELL_TESTS = [
    Action("bell-dc", "DC pulse, 2 s: armature should pull in and hold", "bell:dc", icon="bell"),
    Action("bell-ac", "AC ring, 2 s: one normal ring burst", "bell:ac", icon="bell"),
]
ACTIONS_BY_ID = {
    action.id: action for action in [*(a for section in SECTIONS for a in section.actions), STOP, *BELL_TESTS]
}
TRACK_LABELS = {action.track: action.label for action in ACTIONS_BY_ID.values() if action.track is not None}
EVENTS = EventLog()
PHONE = PhoneState()
STARTED_AT = time.time()


def publish(payload: str) -> None:
    mqtt_publish.single(MQTT_TOPIC, payload, hostname=MQTT_HOST, port=MQTT_PORT)


@app.route("/")
def index():
    return render_template("index.html", sections=SECTIONS, stop=STOP)


@app.post("/api/actions/<action_id>")
def run_action(action_id: str):
    if not request.is_json:
        abort(415)
    action = ACTIONS_BY_ID.get(action_id)
    if action is None:
        abort(404)
    try:
        publish(action.payload)
    except OSError:
        logger.exception("MQTT publish failed for %s", action_id)
        return jsonify({"ok": False, "error": "Phone broker unreachable"}), 502
    logger.info("Sent %s (%s)", action_id, action.payload)
    return jsonify({"ok": True, "label": action.label})


@app.get("/api/events")
def phone_events():
    after = request.args.get("after", type=int)
    if after is None:
        return jsonify({"last_id": EVENTS.last_id, "events": []})
    events = [{"id": event.id, "text": describe(event.status, TRACK_LABELS)} for event in EVENTS.since(after)]
    return jsonify({"last_id": EVENTS.last_id, "events": events})


@app.get("/status")
def status_page():
    return render_template("status.html", bell_tests=BELL_TESTS)


@app.get("/api/status")
def status():
    recent = [
        {"at": event.at, "text": describe(event.status, TRACK_LABELS), "status": event.status}
        for event in EVENTS.recent(20)
    ]
    return jsonify(
        {
            "status": "ok",
            "server_uptime_s": round(time.time() - STARTED_AT),
            "phone": PHONE.snapshot(),
            "events": recent,
        }
    )


def main():
    start_listener(EVENTS, PHONE, MQTT_HOST, MQTT_PORT, MQTT_TOPIC)
    app.run(host="0.0.0.0", port=FLASK_PORT)


if __name__ == "__main__":
    main()
