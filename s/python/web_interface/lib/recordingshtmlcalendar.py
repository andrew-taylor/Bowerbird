from calendar import Calendar, SUNDAY, month_name, day_abbr

class RecordingsHTMLCalendar(Calendar):

	def __init__(self, today, date, recordings_db=None, firstweekday=SUNDAY):
		self.db = recordings_db
		self.today = today
		self.date = date
		print "showing calendar",today, date

		# initialise calendar source
		self.setfirstweekday(firstweekday)
	
	def showMonth(self):
		return self.formatMonth(self.date.year, self.date.month, self.date.day)

	def formatDay(self, themonth, date, num_weeks):
		"""
		Return a day as a table cell.
		"""
		if date.month != themonth:
			html = '<td class="noday' # day outside month
		else:
			html = '<td class="day'

		# if this is today then highlight it
		if date == self.today:
			html += ' today'

		# if this is the selected date then tag it
		if date == self.date:
			html += ' selected'

		html += ('" style="height: %f%%"><div class="day_header">%d</div>' 
				% (90.0 / num_weeks, date.day))

		if self.db:
			for recording in self.db.getRecordings(date):
				html += ('<div class="day_entry">\n'
						'<span class="recording_time">%s</span>\n'
						'<span class="recording_title">%s</span>\n'
						'</div>\n' % (compactTimeFormat(recording.start_time), 
						recording.title))

		return html + '</td>'

	def formatWeek(self, themonth, theweek, num_weeks):
		"""
		Return a complete week as a table row.
		"""
		s = ''.join(self.formatDay(themonth, d, num_weeks) for d in theweek)
		return '<tr>%s</tr>' % s

	def formatWeekDay(self, day):
		"""
		Return a weekday name as a table header.
		"""
		return '<th class="day">%s</th>' % day_abbr[day]

	def formatWeekHeader(self):
		"""
		Return a header for a week as a table row.
		"""
		s = ''.join(self.formatWeekDay(i) for i in self.iterweekdays())
		return '<tr>%s</tr>' % s

	def formatMonthName(self, theyear, themonth, theday, withyear=True, 
			withlinks=True):
		"""
		Return a month name as a table row.
		"""
		if withyear:
			title = '%s %s' % (month_name[themonth], theyear)
		else:
			title = '%s' % month_name[themonth]

		if withlinks:
			prev_month = themonth - 1
			prev_month_year = theyear
			next_month = themonth + 1
			next_month_year = theyear
			if prev_month == 0:
				prev_month = 12
				prev_month_year -= 1
			elif next_month == 13:
				next_month = 1
				next_month_year += 1

			title = ('<a href="?day=%(d)d&month=%(m)d&year=%(py)d">'
					'&lt;&lt;</a>%(s)s'
					'<a href="?day=%(d)d&month=%(pm)d&year=%(pmy)d">'
					'&lt;</a>%(s)s'
					'%(title)s'
					'%(s)s<a href="?day=%(d)d&month=%(nm)d&year=%(nmy)d">'
					'&gt;</a>'
					'%(s)s<a href="?day=%(d)d&month=%(m)d&year=%(ny)d">'
					'&gt;&gt;</a>'
					% {'d': theday, 'm': themonth, 'py': theyear-1, 
						's': '&nbsp;&nbsp;', 'pm': prev_month, 
						'pmy': prev_month_year, 'title': title,
						'nm': next_month, 'nmy': next_month_year, 
						'ny': theyear+1	})

		return '<tr><th colspan="7" class="month">%s</th></tr>' % title

	def formatMonth(self, theyear, themonth, theday, withyear=True):
		"""
		Return a formatted month as a table.
		"""
		html = ('<table class="month">\n%s\n%s\n'
				% (self.formatMonthName(theyear, themonth, theday, 
						withyear=withyear), self.formatWeekHeader()))
		weeks = self.monthdatescalendar(theyear, themonth)
		for week in weeks:
			html += self.formatWeek(themonth, week, len(weeks)) + '\n'
		return html + '</table>\n'

def compactTimeFormat(time):
	if time.minute == 0:
		return time.strftime('%H')
	return time.strftime('%H:%M')
