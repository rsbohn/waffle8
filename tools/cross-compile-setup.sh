#!/usr/bin/env bash
set -euo pipefail

APT_PACKAGES=(
  build-essential
  cmake
  ninja-build
  python3
  python3-pip
  git
  gcc-arm-none-eabi
  binutils-arm-none-eabi
  libnewlib-arm-none-eabi
  gdb-multiarch
  ccache
)

echo "[pico-setup] installing toolchain packages via apt"
sudo apt-get update
sudo apt-get install -y "${APT_PACKAGES[@]}"
sudo apt-get clean

install_repo() {
  local url=$1
  local dest=$2
  if [ -d "${dest}/.git" ]; then
    echo "[pico-setup] updating ${dest}"
    sudo git -C "${dest}" pull --ff-only
  else
    echo "[pico-setup] cloning ${url} -> ${dest}"
    sudo git clone --depth 1 "${url}" "${dest}"
  fi
}

# Core SDK
PICO_SDK_PATH=${PICO_SDK_PATH:-/opt/pico-sdk}
install_repo https://github.com/raspberrypi/pico-sdk "${PICO_SDK_PATH}"
sudo git -C "${PICO_SDK_PATH}" submodule update --init --recursive

# Extras the Apple II reference uses (DVI helpers, etc.)
PICO_EXTRAS_PATH=${PICO_EXTRAS_PATH:-/opt/pico-extras}
install_repo https://github.com/raspberrypi/pico-extras "${PICO_EXTRAS_PATH}"
sudo git -C "${PICO_EXTRAS_PATH}" submodule update --init --recursive

# Playground contains additional display/keybd samples; handy for reference
PICO_PLAYGROUND_PATH=${PICO_PLAYGROUND_PATH:-/opt/pico-playground}
install_repo https://github.com/raspberrypi/pico-playground "${PICO_PLAYGROUND_PATH}"
sudo git -C "${PICO_PLAYGROUND_PATH}" submodule update --init --recursive

# Optional: clone the Fruit Jam Apple II reference if you know its URL.
# export APPLE2_FRUITJAM_REPO=https://github.com/<org>/<repo>.git before running
if [ -n "${APPLE2_FRUITJAM_REPO:-}" ]; then
  APPLE2_REF_PATH=${APPLE2_REF_PATH:-/opt/fruitjam-apple2}
  install_repo "${APPLE2_FRUITJAM_REPO}" "${APPLE2_REF_PATH}"
fi

# Make the paths available to every shell session.
sudo tee /etc/profile.d/pico-sdk.sh >/dev/null <<EOF
export PICO_SDK_PATH=${PICO_SDK_PATH}
export PICO_EXTRAS_PATH=${PICO_EXTRAS_PATH}
export PICO_PLAYGROUND_PATH=${PICO_PLAYGROUND_PATH}
if [ -d "${APPLE2_REF_PATH:-}" ]; then
  export FRUITJAM_APPLE2_REF=${APPLE2_REF_PATH}
fi
EOF
sudo chmod 644 /etc/profile.d/pico-sdk.sh

echo "[pico-setup] verifying toolchain availability"
arm-none-eabi-gcc --version
cmake --version
ninja --version
python3 --version

echo "[pico-setup] setup complete"
