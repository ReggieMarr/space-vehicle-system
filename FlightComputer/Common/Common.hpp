#ifndef COMMON_H_
#define COMMON_H_

#define FW_CHECK(cond, err_string, ...)                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      Fw::Logger::log(err_string);                                             \
      __VA_ARGS__;                                                             \
      break;                                                                   \
    }                                                                          \
  } while (0);

#endif // COMMON_H_
