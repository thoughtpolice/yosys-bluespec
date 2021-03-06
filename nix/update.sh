#! /usr/bin/env nix-shell
#! nix-shell -i bash -p jq curl

## Utility to automatically update nixpkgs.json quickly and easily from either
## the Nixpkgs upstream or a custom fork. Runs interactively using `nix-shell`
## for zero-install footprint.
set -e

API="https://api.github.com/repos"
ORG=${ORG:-"nixos"}
REPO=${REPO:-"nixpkgs"}
BRANCH=${BRANCH:-"nixpkgs-unstable"}
URL="https://github.com/${ORG}/${REPO}"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
[ ! -e "$SCRIPT_DIR/nixpkgs.json" ] && \
  >&2 echo "ERROR: nixpkgs.json must be located next to this update script!" && \
  exit 1

if [[ "x$1" == "x" ]]; then
  echo -n "No explicit revision given, so using latest commit from '${ORG}/${REPO}@${BRANCH}' ... "
  REV=$(curl -s "${API}/nixos/${REPO}/commits/${BRANCH}" | jq -r '.sha')
  echo "OK, got ${REV:0:6}"
else
  if [[ "x$2" == "x" ]]; then
    REV="$1"
    echo "Custom revision (but no repo) provided, using ${URL}"
  else
    REV="$2"
    URL="$1"
    echo "Custom revision in upstream ${URL} will be used"
  fi
fi

DOWNLOAD="$URL/archive/$REV.tar.gz"
echo "Updating to nixpkgs revision ${REV:0:6} from $URL"
SHA256=$(nix-prefetch-url --unpack "$DOWNLOAD")

cat > "$SCRIPT_DIR/nixpkgs.json" <<EOF
{
  "url":    "$DOWNLOAD",
  "rev":    "$REV",
  "sha256": "$SHA256"
}
EOF

echo "Updated nixpkgs.json"
