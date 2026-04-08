#!/bin/bash
# =============================================================================
# YAG Centralized Sync Test Script (SSH/SCP Edition)
#
# Tests push/pull between two local directories using SSH to localhost.
# Requires: SSH access to yourself (ssh localhost) with key-based auth.
#
# Usage:
#   ./test_central.sh [path-to-yag-binary]
#
# Default binary: ./build/yag
# =============================================================================

set -e

YAG="${1:-./build/yag}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
USER_A="$SCRIPT_DIR/_test_user_a"
USER_B="$SCRIPT_DIR/_test_user_b"
PROJECT="test_sync_$$"
REMOTE_SPEC="$USER@localhost"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; exit 1; }
section() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

# Cleanup on exit
cleanup() {
    rm -rf "$USER_A" "$USER_B" 2>/dev/null
    # Clean up central repo on remote (which is localhost)
    ssh localhost "rm -rf ~/yag-central/projects/$PROJECT" 2>/dev/null || true
}
trap cleanup EXIT

cleanup  # ensure clean start

# ------------------------------------------------------------------
section "Prerequisites: SSH to localhost"
# ------------------------------------------------------------------
ssh -o BatchMode=yes -o ConnectTimeout=5 localhost "echo ok" > /dev/null 2>&1 \
    || fail "Cannot SSH to localhost. Set up key-based auth first:\n  ssh-keygen && ssh-copy-id localhost"
pass "SSH to localhost works"

# ------------------------------------------------------------------
section "User A: init + remote set + add + commit + push"
# ------------------------------------------------------------------
mkdir -p "$USER_A" && cd "$USER_A"
$YAG init "$PROJECT"
$YAG remote set "$REMOTE_SPEC"
echo "hello world" > file1.txt
echo "int main() {}" > main.cpp
$YAG add .
$YAG commit "A: initial commit"
$YAG push
pass "User A pushed initial commit via SCP"

# Verify files arrived on remote
ssh localhost "test -d ~/yag-central/projects/$PROJECT/objects" \
    || fail "Remote objects/ dir not created"
ssh localhost "test -d ~/yag-central/projects/$PROJECT/commits" \
    || fail "Remote commits/ dir not created"
ssh localhost "test -f ~/yag-central/projects/$PROJECT/branches/main" \
    || fail "Remote branch pointer not created"
pass "Remote directory structure verified"

# ------------------------------------------------------------------
section "User A: idempotent push (should be zero-transfer)"
# ------------------------------------------------------------------
cd "$USER_A"
OUTPUT=$($YAG push 2>&1)
echo "$OUTPUT" | grep -q "Already up to date" || fail "Idempotent push not detected"
pass "Push is idempotent — 'Already up to date'"

# ------------------------------------------------------------------
section "User B: init + remote set + pull (receives A's files)"
# ------------------------------------------------------------------
mkdir -p "$USER_B" && cd "$USER_B"
$YAG init "$PROJECT"
$YAG remote set "$REMOTE_SPEC"
$YAG pull

# Verify files actually arrived
[[ -f file1.txt ]] || fail "file1.txt missing after pull"
[[ -f main.cpp ]]  || fail "main.cpp missing after pull"
[[ "$(cat file1.txt)" == "hello world" ]] || fail "file1.txt content wrong"
pass "User B pulled and got correct files via SCP"

# ------------------------------------------------------------------
section "User B: modify + commit + push"
# ------------------------------------------------------------------
cd "$USER_B"
echo "modified by B" > file1.txt
$YAG add file1.txt
$YAG commit "B: updated file1"
$YAG push
pass "User B pushed update via SCP"

# ------------------------------------------------------------------
section "User A: pull (fast-forward, no conflict)"
# ------------------------------------------------------------------
cd "$USER_A"
$YAG pull
[[ "$(cat file1.txt)" == "modified by B" ]] || fail "file1.txt not updated after pull"
pass "User A fast-forwarded successfully"

# ------------------------------------------------------------------
section "User A: status shows clean"
# ------------------------------------------------------------------
cd "$USER_A"
STATUS=$($YAG status)
echo "$STATUS" | grep -q "Nothing to commit" || fail "Status not clean after pull"
pass "Working tree clean after pull"

# ------------------------------------------------------------------
section "Empty commit blocked"
# ------------------------------------------------------------------
cd "$USER_A"
OUTPUT=$($YAG commit "should fail" 2>&1) || true
echo "$OUTPUT" | grep -q "No changes to commit" || fail "Empty commit not blocked"
pass "Empty commit correctly blocked"

# ------------------------------------------------------------------
section "Remote show"
# ------------------------------------------------------------------
cd "$USER_A"
OUTPUT=$($YAG remote show 2>&1)
echo "$OUTPUT" | grep -q "localhost" || fail "Remote show missing host"
echo "$OUTPUT" | grep -q "$USER" || fail "Remote show missing user"
pass "yag remote show displays config correctly"

# ------------------------------------------------------------------
section "Conflict detection"
# ------------------------------------------------------------------
# User A modifies and commits locally
cd "$USER_A"
echo "A's version" > file1.txt
$YAG add .
$YAG commit "A: my version"

# User B modifies, commits, and pushes
cd "$USER_B"
echo "B's version" > file1.txt
$YAG add .
$YAG commit "B: my version"
$YAG push

# User A tries to pull → should detect conflict
cd "$USER_A"
OUTPUT=$($YAG pull 2>&1) || true
echo "$OUTPUT" | grep -q "Conflict detected" || fail "Conflict not detected"
pass "Conflict correctly detected"

# ------------------------------------------------------------------
section "Log shows full history"
# ------------------------------------------------------------------
cd "$USER_A"
LOG=$($YAG log)
echo "$LOG" | grep -q "A: my version"  || fail "Missing A's commit in log"
echo "$LOG" | grep -q "B: updated file1" || fail "Missing B's commit in log"
echo "$LOG" | grep -q "A: initial commit" || fail "Missing initial commit in log"
pass "Log shows complete history"

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  ALL TESTS PASSED (SSH/SCP)${NC}"
echo -e "${GREEN}============================================${NC}"
