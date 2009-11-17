import re
TIME_PATTERN = '([SR]?) *([+-]?[0-9]{1,2}):([0-9]{2})'
SCHEDULE_PATTERN = '^ *%s *- *%s *$' % (TIME_PATTERN, TIME_PATTERN)
SCHEDULE_RE = re.compile(SCHEDULE_PATTERN)
