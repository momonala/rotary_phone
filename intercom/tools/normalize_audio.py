# /// script
# dependencies = ["typer"]
# ///
"""Level-match the intercom tracks by their energy in the band the intercom speaker plays, then write them to the SD card folder.

Usage: uv run intercom/tools/normalize_audio.py [--target DB] [--source DIR] [--dest DIR]
"""

import re
import subprocess
import tempfile
from pathlib import Path
from typing import Annotated

import typer

MEDIA = Path(__file__).resolve().parent.parent / "media"
BAND_FILTER = "highpass=f=300,lowpass=f=3400"
PEAK_LIMIT = 0.84  # -1.5 dBFS


def ffmpeg(*args: str) -> str:
    result = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", *args], capture_output=True, text=True, check=True
    )
    return result.stderr


def band_level(path: Path) -> float:
    output = ffmpeg("-i", str(path), "-af", f"{BAND_FILTER},volumedetect", "-f", "null", "-")
    return float(re.search(r"mean_volume: (-?[\d.]+) dB", output).group(1))


def peak_level(path: Path) -> float:
    output = ffmpeg("-i", str(path), "-af", "volumedetect", "-f", "null", "-")
    return float(re.search(r"max_volume: (-?[\d.]+) dB", output).group(1))


def encode(source: Path, dest: Path, gain_db: float) -> None:
    ffmpeg(
        "-y",
        "-i",
        str(source),
        "-af",
        f"volume={gain_db:.2f}dB,alimiter=limit={PEAK_LIMIT}:level=disabled",
        "-ar",
        "44100",
        "-ac",
        "2",
        "-b:a",
        "192k",
        "-map_metadata",
        "-1",
        str(dest),
    )


def main(
    target: Annotated[float, typer.Option(help="Target mean level in dB within the speaker band.")] = -14.0,
    source: Annotated[Path, typer.Option(help="Folder of unnormalized tracks.")] = MEDIA
    / "originals"
    / "unnormalized",
    dest: Annotated[Path, typer.Option(help="SD card folder to write to.")] = MEDIA / "01",
) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    for track in sorted(source.glob("*.mp3")):
        gain = target - band_level(track)
        with tempfile.TemporaryDirectory() as tmp:
            trial = Path(tmp) / track.name
            for _ in range(3):
                encode(track, trial, gain)
                gain += target - band_level(trial)
        out = dest / track.name
        encode(track, out, gain)
        print(
            f"{track.name:32} gain {gain:+5.1f} dB  band {band_level(out):6.1f} dB  peak {peak_level(out):5.1f} dB"
        )


if __name__ == "__main__":
    typer.run(main)
