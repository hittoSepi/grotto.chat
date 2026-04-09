#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
config="${2:-Release}"
skip_build="${SKIP_BUILD:-0}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "$build_dir" != /* ]]; then
  build_dir="$repo_root/$build_dir"
fi

if [[ ! -d "$build_dir" ]]; then
  echo "Build directory not found: $build_dir" >&2
  exit 1
fi

if [[ "$skip_build" != "1" ]]; then
  echo "Building grotto-client..."
  cmake --build "$build_dir" --target grotto-client
  echo "Running check target..."
  cmake --build "$build_dir" --target check
fi

output_dir="$build_dir/$config"
if [[ ! -d "$output_dir" ]]; then
  output_dir="$build_dir"
fi

client_bin="$output_dir/grotto-client"
if [[ ! -f "$client_bin" ]]; then
  echo "Client binary not found: $client_bin" >&2
  exit 1
fi

qa_dir="$build_dir/qa-error-scenarios"
mkdir -p "$qa_dir/downloads"

qa_config="$qa_dir/client.toml"
sed \
  -e 's/^auto_reconnect[[:space:]]*=.*/auto_reconnect = true/' \
  -e 's/^reconnect_delay_sec[[:space:]]*=.*/reconnect_delay_sec = 1/' \
  -e 's/^enabled[[:space:]]*=.*# Enable link previews$/enabled = false           # Enable link previews/' \
  -e 's/^inline_images[[:space:]]*=.*/inline_images = false          # Keep QA runs deterministic/' \
  "$repo_root/config/client.toml.example" > "$qa_config"

server_bin=""
for candidate in \
  "$repo_root/../grotto-chat-server/build/grotto-chat-server" \
  "$repo_root/../grotto-chat-server/build-codex-check/grotto-chat-server"
do
  if [[ -f "$candidate" ]]; then
    server_bin="$candidate"
    break
  fi
done

checklist="$qa_dir/ERROR-QA-CHECKLIST.txt"
{
  echo "Grotto Client Error Scenario QA"
  echo
  echo "Prepared config: $qa_config"
  echo "Client binary:    $client_bin"
  if [[ -n "$server_bin" ]]; then
    echo "Server binary:    $server_bin"
  else
    echo "Server binary:    <not found automatically>"
  fi
  echo
  echo "Recommended launch commands:"
  echo "  Client:"
  echo "    \"$client_bin\" --config \"$qa_config\""
  echo "  Server:"
  if [[ -n "$server_bin" ]]; then
    echo "    \"$server_bin\""
  else
    echo "    Build/start the server manually before running the client."
  fi
  echo
  echo "Checklist:"
  echo "1. Start server, then start client with the QA config above and log in."
  echo "2. Join a channel or DM, then stop the server process."
  echo "3. Verify the client shows reconnecting state instead of freezing or exiting."
  echo "4. While reconnecting, send a message and verify the status bar shows queued outbound work."
  echo "5. Start the server again and verify the client re-authenticates and flushes the queued message."
  echo "6. Trigger a DM session repair case if available and verify reconnect does not drop the repair request immediately."
  echo "7. Join a voice room or start a direct call, then exit the client with /quit."
  echo "8. Verify the client process exits promptly without hanging after voice shutdown."
  echo "9. Start an upload or download, then exit the client with /quit."
  echo "10. Verify the client exits promptly and the next launch starts cleanly without stuck transfers."
  echo "11. Run the same exit test from the window close action and from Settings -> Logout."
  echo
  echo "Reference doc: docs/error-scenario-qa.md"
} > "$checklist"

echo
echo "Prepared QA workspace: $qa_dir"
echo "Checklist written to:  $checklist"
echo
cat "$checklist"
