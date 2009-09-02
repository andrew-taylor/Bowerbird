#!/usr/bin/env python
import sys, sqlite3, re, array, numpy
from database import *

def insert_unit_attributes_db_command(table):
    return ('INSERT OR REPLACE INTO %s values(?' + (len(UNIT_ATTRIBUTE_FIELDS)-1) * ',?' +')') % table

def update_db_unit_attributes(db, sources_hash, table):
    c = db.execute('SELECT * FROM units')
    row = c.fetchone()
    attributes = []
    i = 0
    while row:
        db.execute(insert_unit_attributes_db_command(table), Unit(row,sources_hash=sources_hash).to_attribute_db_columns())
        row = c.fetchone()
        i += 1
        if i % 10000 == 0: db.commit()
    db.commit()
if __name__ == "__main__":
    for db_name in sys.argv[1:]:
        db = sqlite3.connect(db_name)
        sources_hash = {}
        sources=read_db(db=db,sources_hash = sources_hash)
        db.execute(CREATE_ATTRIBUTE_TABLE)
        update_db_unit_attributes(db, sources_hash, ATTRIBUTE_TABLE_NAME)
        db.close()
