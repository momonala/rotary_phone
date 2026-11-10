import pytest

from rotary_phone.web import app as app_module


@pytest.fixture
def published(monkeypatch):
    sent = []
    monkeypatch.setattr(app_module, "publish", sent.append)
    return sent


@pytest.fixture
def client():
    return app_module.app.test_client()


def test_index_lists_every_action(client):
    html = client.get("/").get_data(as_text=True)
    remote_actions = [action for section in app_module.SECTIONS for action in section.actions]
    for action in [*remote_actions, app_module.STOP]:
        assert f'data-action="{action.id}"' in html


def test_every_sd_card_track_has_a_button():
    tracks = {a.track for a in app_module.ACTIONS_BY_ID.values() if a.track is not None}
    assert tracks == set(range(15)) | {30, 31, 32, 33, 98, 99}


@pytest.mark.parametrize(
    ("action_id", "payload"),
    [("track-30", "30"), ("sound-14", "play:14"), ("stop", "stop"), ("bell-dc", "bell:dc"), ("bell-ac", "bell:ac")],
)
def test_action_publishes_its_payload(client, published, action_id, payload):
    response = client.post(f"/api/actions/{action_id}", json={})
    assert response.status_code == 200
    assert published == [payload]


def test_unknown_action_is_404(client, published):
    assert client.post("/api/actions/nope", json={}).status_code == 404
    assert published == []


def test_non_json_post_is_rejected(client, published):
    assert client.post("/api/actions/track-30", data="x").status_code == 415
    assert published == []


def test_broker_down_returns_502(client, monkeypatch):
    def fail(_payload):
        raise ConnectionRefusedError

    monkeypatch.setattr(app_module, "publish", fail)
    response = client.post("/api/actions/stop", json={})
    assert response.status_code == 502
    assert response.get_json()["ok"] is False


def test_events_first_poll_only_returns_cursor(client, monkeypatch):
    log = app_module.EventLog()
    log.add("ringing")
    monkeypatch.setattr(app_module, "EVENTS", log)
    assert client.get("/api/events").get_json() == {"last_id": 1, "events": []}


def test_events_after_cursor_are_described(client, monkeypatch):
    log = app_module.EventLog()
    monkeypatch.setattr(app_module, "EVENTS", log)
    for status in ("ringing", "picked_up", "playing:33", "dialed:911"):
        log.add(status)
    body = client.get("/api/events?after=1").get_json()
    assert [event["text"] for event in body["events"]] == [
        "Picked up",
        "Playing Taken phone speech",
        "Dialed 911",
    ]
    assert body["last_id"] == 4


def test_status_page_lists_every_bell_test(client):
    html = client.get("/status").get_data(as_text=True)
    for action in app_module.BELL_TESTS:
        assert f'data-action="{action.id}"' in html


def test_api_status_reports_phone_health_and_events(client, monkeypatch):
    phone = app_module.PhoneState()
    phone.set_availability("online")
    phone.set_health({"hook_lifted": False, "dfplayer_volume": 15})
    events = app_module.EventLog()
    events.add("ringing")
    events.add("picked_up")
    monkeypatch.setattr(app_module, "PHONE", phone)
    monkeypatch.setattr(app_module, "EVENTS", events)

    body = client.get("/api/status").get_json()

    assert body["phone"]["status"] == "broker_down"
    assert body["phone"]["availability"] == "online"
    assert body["phone"]["health"]["dfplayer_volume"] == 15
    assert [event["text"] for event in body["events"]] == ["Picked up", "Ringing"]
