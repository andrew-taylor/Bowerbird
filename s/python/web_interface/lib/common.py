import cherrypy

# zeroconf type. Using HTTP means that normal zeroconf-supporting web browsers
# will see our bowerbird systems.
ZEROCONF_TYPE = '_http._tcp'
# zeroconf text to identify bowerbird
ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD = 'This is a bowerbird system.'

# cherrypy sessions for recordings page
SESSION_YEAR_KEY = 'year'
SESSION_MONTH_KEY = 'month'
SESSION_FILTER_STATION_KEY = 'filter_station'
SESSION_FILTER_TITLE_KEY = 'filter_title'
SESSION_FILTER_START_KEY = 'filter_start'
SESSION_FILTER_FINISH_KEY = 'filter_finish'
SESSION_DATE_KEY = 'date'
SESSION_RECORD_ID_KEY = 'record_id'
SESSION_STATION_NAME_KEY = 'station_name'
SESSION_STATION_ADDRESS_KEY = 'station_address'

NO_FILTER_STATION = 'All Stations'
NO_FILTER_TITLE = 'All Recordings'

def hasSession(key):
    return key in cherrypy.session

def setSession(key, value):
    cherrypy.session[key] = value

def getSession(key):
    if cherrypy.session.has_key(key):
        return cherrypy.session[key]
    return None

def clearSession(key):
    if cherrypy.session.has_key(key):
        del cherrypy.session[key]

def printSession():
    print 'CherryPy Session:'
    for key in (SESSION_YEAR_KEY, SESSION_MONTH_KEY, SESSION_FILTER_STATION_KEY,
            SESSION_FILTER_TITLE_KEY, SESSION_FILTER_START_KEY,
            SESSION_FILTER_FINISH_KEY, SESSION_DATE_KEY, SESSION_RECORD_ID_KEY,
            SESSION_STATION_NAME_KEY, SESSION_STATION_ADDRESS_KEY):
        if hasSession(key):
            print '\t%s: %s' % (key, getSession(key))

def calculateActionForSelected(selected_recordings=None, connected_station=None,
        transfer_queue_ids=[]):
    if not selected_recordings:
        return '', 0

    # check if all files are available and whether the missing ones
    # are retrievable
    found_exportable = False
    found_retrievable = False
    found_retrieving = False
    found_unretrievable = False
    export_size = 0
    retrieve_size = 0
    for recording in selected_recordings:
        if recording.fileExists():
            export_size += recording.size
            found_exportable = True
        elif recording.id in transfer_queue_ids:
            found_retrieving = True
        elif connected_station and recording.station == connected_station:
            retrieve_size += recording.size
            found_retrievable = True
        else:
            found_unretrievable = True
    # set an appropriate button
    if found_retrievable:
        if found_unretrievable:
            return 'retrieve_available', retrieve_size
        else:
            return 'retrieve', retrieve_size
    elif found_exportable:
        if found_unretrievable:
            return 'export_available', export_size
        else:
            return 'export', export_size
    elif found_retrieving:
        return 'already_retrieving', 0
    elif found_unretrievable:
        return 'retreive_unavailable', 0
    else:
        return 'disabled_export', 0
