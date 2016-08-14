#ifndef tealtree_log_trivial_h
#define tealtree_log_trivial_h

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

void init_logger();
void destroy_logger();
#endif
