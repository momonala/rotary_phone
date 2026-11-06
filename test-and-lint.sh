#!/bin/bash
# Pre-commit hook: tests, ruff format, ruff check

set -e

echo "🧪 Running tests..."
uv run --with pytest --with pytest-cov pytest

echo "🖤 Running ruff format..."
if ! uvx ruff format --check rotary_phone/web; then
    echo "❌ Ruff found formatting issues. To auto fix, run:"
    echo -e "\033[32muvx ruff format rotary_phone/web\033[0m"
    exit 1
fi

echo "🧼 Running ruff check..."
if ! uvx ruff check rotary_phone/web; then
    echo "❌ Ruff found linting issues. To auto fix, run:"
    echo -e "\033[32muvx ruff check rotary_phone/web --fix\033[0m"
    exit 1
fi


echo "🔍 Checking if template files that block installation have been updated or deleted."
if [ -f install/projects_.service ]; then
    echo "❌ install/projects_.service needs to be renamed or deleted"
    exit 1
fi


echo "✅ Pre-commit checks passed!"
