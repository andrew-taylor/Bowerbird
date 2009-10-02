import re
time_pattern = '([SR]?) *([+-]?[0-9]{1,2}):([0-9]{2})'
schedule_pattern = '^ *%s *- *%s *$' % (time_pattern, time_pattern)
schedule_re = re.compile(schedule_pattern)
