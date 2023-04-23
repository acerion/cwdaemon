#include "log.h"




/**
   @brief Get string label corresponding to given log priority
*/
const char * log_get_priority_label(int priority)
{
   switch (priority) {
   case LOG_ERR:
      return "ERR";
   case LOG_WARNING:
      return "WARN";
   case LOG_INFO:
      return "INFO";
   case LOG_DEBUG:
      return "DEBUG";
   case LOG_EMERG:  /* LOG_EMERG is not used by this program. */
   case LOG_ALERT:  /* LOG_ALERT is not used by this program. */
   case LOG_CRIT:   /* LOG_CRIT is not used by this program. */
   case LOG_NOTICE: /* LOG_NOTICE is not used by this program. "n" passed in command line to "-y" switch means "none". */
   default:
      return "--";
   }
}




