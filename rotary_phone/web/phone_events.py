"""What the phone reports over MQTT: call events, its health snapshot, and whether it is online."""

import json
import logging
import time
from collections import deque
from dataclasses import dataclass
from enum import StrEnum
from threading import Lock

import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)

HEARTBEAT_STALE_S = 75

STATUS_TEXT = {
    "online": "Phone online",
    "ringing": "Ringing",
    "picked_up": "Picked up",
    "hung_up": "Hung up",
    "no_answer": "No answer",
    "stopped": "Stopped",
    "finished": "Finished",
    "busy": "Handset already off the hook",
    "bell_dc": "Bell: DC on",
    "bell_ac": "Bell: ringing",
    "bell_off": "Bell off",
}


@dataclass(frozen=True)
class PhoneEvent:
    id: int
    status: str
    at: float


class EventLog:
    def __init__(self, size: int = 50):
        self._events: deque[PhoneEvent] = deque(maxlen=size)
        self._last_id = 0
        self._lock = Lock()

    def add(self, status: str) -> PhoneEvent:
        with self._lock:
            self._last_id += 1
            event = PhoneEvent(self._last_id, status, time.time())
            self._events.append(event)
            return event

    def since(self, after: int) -> list[PhoneEvent]:
        with self._lock:
            return [event for event in self._events if event.id > after]

    def recent(self, count: int) -> list[PhoneEvent]:
        with self._lock:
            return list(self._events)[-count:][::-1]

    @property
    def last_id(self) -> int:
        with self._lock:
            return self._last_id


class DeviceStatus(StrEnum):
    ONLINE = "online"
    STALE = "stale"
    OFFLINE = "offline"
    BROKER_DOWN = "broker_down"


def device_status(availability: str | None, health_age_s: int | None, broker_connected: bool) -> DeviceStatus:
    if not broker_connected:
        return DeviceStatus.BROKER_DOWN
    if availability != "online":
        return DeviceStatus.OFFLINE
    if health_age_s is None or health_age_s > HEARTBEAT_STALE_S:
        return DeviceStatus.STALE
    return DeviceStatus.ONLINE


class PhoneState:
    def __init__(self):
        self._lock = Lock()
        self.availability: str | None = None
        self.availability_at: float | None = None
        self.health: dict | None = None
        self.health_at: float | None = None
        self.broker_connected = False

    def set_availability(self, value: str) -> None:
        with self._lock:
            self.availability, self.availability_at = value, time.time()

    def set_health(self, health: dict) -> None:
        with self._lock:
            self.health, self.health_at = health, time.time()

    def snapshot(self) -> dict:
        now = time.time()
        with self._lock:
            health_age_s = _age(self.health_at, now)
            return {
                "status": device_status(self.availability, health_age_s, self.broker_connected),
                "broker_connected": self.broker_connected,
                "availability": self.availability,
                "availability_age_s": _age(self.availability_at, now),
                "health": self.health,
                "health_age_s": health_age_s,
            }


def _age(at: float | None, now: float) -> int | None:
    return None if at is None else round(now - at)


def describe(status: str, track_labels: dict[int, str]) -> str:
    kind, _, value = status.partition(":")
    if kind == "playing" and value.isdigit():
        return f"Playing {track_labels.get(int(value), f'track {value}')}"
    if kind == "dialed":
        return f"Dialed {value}"
    return STATUS_TEXT.get(status, status)


def start_listener(events: EventLog, state: PhoneState, host: str, port: int, base_topic: str) -> mqtt.Client:
    topics = {
        f"{base_topic}/status": lambda payload: events.add(payload),
        f"{base_topic}/availability": state.set_availability,
        f"{base_topic}/state": lambda payload: state.set_health(json.loads(payload)),
    }
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(client, _userdata, _flags, reason_code, _properties):
        logger.info("Phone listener connected (%s)", reason_code)
        state.broker_connected = not reason_code.is_failure
        for topic in topics:
            client.subscribe(topic)

    def on_disconnect(_client, _userdata, _flags, reason_code, _properties):
        logger.warning("Phone listener disconnected (%s)", reason_code)
        state.broker_connected = False

    def on_message(_client, _userdata, message):
        payload = message.payload.decode(errors="replace").strip()
        logger.info("%s: %s", message.topic, payload)
        try:
            topics[message.topic](payload)
        except (KeyError, ValueError):
            logger.exception("Could not handle %s", message.topic)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.connect_async(host, port)
    client.loop_start()
    return client
