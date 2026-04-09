#pragma once

namespace yag::core {

// Repository health check: validates HEAD, branches, commits, and object hashes
void run_doctor();

} // namespace yag::core
