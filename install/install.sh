set -e

if ! command -v uv &> /dev/null; then
    curl -LsSf https://astral.sh/uv/install.sh | sh
    export PATH="$HOME/.local/bin:$HOME/.cargo/bin:$PATH"
fi

uv sync

service_name=projects_$(uv run config --project-name)
service_port=$(uv run config --flask-port)
domain=$(uv run config --domain)

uv run config --all

sudo cp install/${service_name}.service /lib/systemd/system/${service_name}.service
sudo chmod 644 /lib/systemd/system/${service_name}.service
sudo systemctl daemon-reload
sudo systemctl enable ${service_name}.service
sudo systemctl restart ${service_name}.service
sudo systemctl status ${service_name}.service --no-pager

/home/mnalavadi/add_cloudflared_service.sh ${domain} ${service_port}
cloudflared tunnel route dns raspberrypi-tunnel ${domain}
sudo systemctl restart cloudflared

echo "Installed ${service_name} at https://${domain}"
