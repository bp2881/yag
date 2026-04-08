#!/bin/bash
# =============================================================================
# YAG Centralized Sync Test Script
# Simulates two users sharing a project through ~/.yag-central/
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
    rm -rf "$USER_A" "$USER_B" "$HOME/.yag-central/projects/$PROJECT" 2>/dev/null
}
trap cleanup EXIT

cleanup  # ensure clean start

# ------------------------------------------------------------------
section "User A: init + add + commit + push"
# ------------------------------------------------------------------
mkdir -p "$USER_A" && cd "$USER_A"
$YAG init "$PROJECT"
echo "hello world" > file1.txt
echo "int main() {}" > main.cpp
$YAG add .
$YAG commit "A: initial commit"
$YAG push
pass "User A pushed initial commit"

# ------------------------------------------------------------------
section "User B: init + pull (receives A's files)"
# ------------------------------------------------------------------
mkdir -p "$USER_B" && cd "$USER_B"
$YAG init "$PROJECT"
$YAG pull

# Verify files actually arrived
[[ -f file1.txt ]] || fail "file1.txt missing after pull"
[[ -f main.cpp ]]  || fail "main.cpp missing after pull"
[[ "$(cat file1.txt)" == "hello world" ]] || fail "file1.txt content wrong"
pass "User B pulled and got correct files"

# ------------------------------------------------------------------
section "User B: modify + commit + push"
# ------------------------------------------------------------------
cd "$USER_B"
echo "modified by B" > file1.txt
$YAG add file1.txt
$YAG commit "B: updated file1"
$YAG push
pass "User B pushed update"

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
echo -e "${GREEN}  ALL TESTS PASSED${NC}"
echo -e "${GREEN}============================================${NC}"
