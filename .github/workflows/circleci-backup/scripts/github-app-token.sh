#!/usr/bin/env bash
# Generates an installation token of the format bot (a GitHub App, see docs/contributing/repository-setup.md)
# and exports GH_TOKEN and BOT_SLUG to the next steps through $BASH_ENV.
# Needs FORMAT_BOT_CLIENT_ID, FORMAT_BOT_PRIVATE_KEY (the .pem file, raw or base64) and GITHUB_REPOSITORY.
set -euo pipefail

key_file=$(mktemp)
trap 'rm -f "$key_file"' EXIT
if printf '%s' "$FORMAT_BOT_PRIVATE_KEY" | grep -q 'BEGIN'; then
  printf '%s\n' "$FORMAT_BOT_PRIVATE_KEY" > "$key_file"
else
  printf '%s' "$FORMAT_BOT_PRIVATE_KEY" | base64 -d > "$key_file"
fi

b64url() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }

# JSON Web Token signed with the private key, valid for 10 minutes (the maximum allowed by GitHub)
now=$(date +%s)
header=$(printf '{"alg":"RS256","typ":"JWT"}' | b64url)
payload=$(printf '{"iat":%d,"exp":%d,"iss":"%s"}' $((now - 60)) $((now + 540)) "$FORMAT_BOT_CLIENT_ID" | b64url)
signature=$(printf '%s.%s' "$header" "$payload" | openssl dgst -sha256 -sign "$key_file" | b64url)
jwt="$header.$payload.$signature"

api() {
  curl -fsSL \
    -H "Authorization: Bearer $jwt" \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    "$@"
}

installation_id=$(api "https://api.github.com/repos/$GITHUB_REPOSITORY/installation" | jq -r .id)
token=$(api -X POST "https://api.github.com/app/installations/$installation_id/access_tokens" | jq -r .token)
slug=$(api "https://api.github.com/app" | jq -r .slug)

{
  echo "export GH_TOKEN='$token'"
  echo "export BOT_SLUG='$slug'"
} >> "$BASH_ENV"
echo "Token generated for ${slug}[bot]."
