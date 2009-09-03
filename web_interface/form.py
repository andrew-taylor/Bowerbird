from formencode import Schema, validators


class ConfigForm(Schema):
	username = validators.UnicodeString(not_empty=True)
	url = validators.URL(not_empty=True, add_http=True, check_exists=True)
	title = validators.UnicodeString(not_empty=True)


class CommentForm(Schema):
	username = validators.UnicodeString(not_empty=True)
	content = validators.UnicodeString(not_empty=True)

