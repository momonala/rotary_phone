import pytest

from rotary_phone.web import phone_events
from rotary_phone.web.phone_events import DeviceStatus
from rotary_phone.web.phone_events import PhoneState
from rotary_phone.web.phone_events import device_status

NOW = 1_700_000_000.0


@pytest.mark.parametrize(
    ("availability", "health_age_s", "broker_connected", "expected"),
    [
        ("online", 75, True, DeviceStatus.ONLINE),
        ("online", 76, True, DeviceStatus.STALE),
        ("online", None, True, DeviceStatus.STALE),
        ("online", 75, False, DeviceStatus.BROKER_DOWN),
        ("online", 76, False, DeviceStatus.BROKER_DOWN),
        ("offline", None, False, DeviceStatus.BROKER_DOWN),
        ("offline", 10, True, DeviceStatus.OFFLINE),
    ],
)
def test_device_status(availability, health_age_s, broker_connected, expected):
    assert device_status(availability, health_age_s, broker_connected) is expected


@pytest.mark.parametrize(
    ("seconds_since_report", "expected"),
    [(75.4, DeviceStatus.ONLINE), (75.6, DeviceStatus.STALE)],
)
def test_snapshot_judges_staleness_on_the_rounded_age(monkeypatch, seconds_since_report, expected):
    monkeypatch.setattr(phone_events.time, "time", lambda: NOW)
    state = PhoneState()
    state.broker_connected = True
    state.availability, state.availability_at = "online", NOW - 300
    state.health, state.health_at = {}, NOW - seconds_since_report

    snapshot = state.snapshot()

    assert snapshot["status"] is expected
    assert snapshot["availability_age_s"] == 300


def test_snapshot_before_first_connect_is_broker_down(monkeypatch):
    monkeypatch.setattr(phone_events.time, "time", lambda: NOW)
    snapshot = PhoneState().snapshot()
    assert snapshot == {
        "status": DeviceStatus.BROKER_DOWN,
        "broker_connected": False,
        "availability": None,
        "availability_age_s": None,
        "health": None,
        "health_age_s": None,
    }
