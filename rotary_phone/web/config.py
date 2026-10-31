import tomllib
from pathlib import Path

import typer

_config_file = Path(__file__).parents[2] / "pyproject.toml"
with _config_file.open("rb") as f:
    _config = tomllib.load(f)

_tool_config = _config["tool"]["config"]

PROJECT_NAME = _config["project"]["name"]
FLASK_PORT = _tool_config["flask_port"]
DOMAIN = _tool_config["domain"]
MQTT_HOST = _tool_config["mqtt_host"]
MQTT_PORT = _tool_config["mqtt_port"]
MQTT_TOPIC = _tool_config["mqtt_topic"]

_VALUES = {
    "project_name": PROJECT_NAME,
    "flask_port": FLASK_PORT,
    "domain": DOMAIN,
    "mqtt_host": MQTT_HOST,
    "mqtt_port": MQTT_PORT,
    "mqtt_topic": MQTT_TOPIC,
}


def config_cli(
    all: bool = typer.Option(False, "--all", help="Show all configuration values"),
    project_name: bool = typer.Option(False, "--project-name"),
    flask_port: bool = typer.Option(False, "--flask-port"),
    domain: bool = typer.Option(False, "--domain"),
) -> None:
    """Expose non-secret configuration defined in pyproject.toml's [tool.config] section."""
    if all:
        for key, value in _VALUES.items():
            typer.echo(f"{key}={value}")
        return
    for is_set, key in ((project_name, "project_name"), (flask_port, "flask_port"), (domain, "domain")):
        if is_set:
            typer.echo(_VALUES[key])
            return
    typer.secho("Error: No config key specified. Use --help to see available options.", fg="red", err=True)
    raise typer.Exit(1)


def main():
    typer.run(config_cli)


if __name__ == "__main__":
    main()
