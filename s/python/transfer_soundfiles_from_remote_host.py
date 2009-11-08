#!/usr/bin/env python
import os,re,bisect,heapq,subprocess
from optparse import OptionParser
from sun import Sun

def find_best_sound_files_to_transfer(remote_files, local_files, how_many_files=10):
    """ choose best files to transfer """
    if not remote_files:
        return []
    files_to_transfer = []
    while len(files_to_transfer) < how_many_files:
        if local_files:
            heap = []
            for rt in remote_files:
                heap.append((smallest_distance(rt, local_files),rt))
            (distance, remote_file_to_transfer) = heapq.nlargest(1, heap)[0]
        else:
            (distance, remote_file_to_transfer) = 1,remote_files[len(remote_files)//2]
        if distance == 0:
            return
#        print (distance, remote_file_to_transfer)
        bisect.insort(local_files, remote_file_to_transfer)
#        print local_files
        files_to_transfer.append(remote_file_to_transfer)
    return [f[1] for f in files_to_transfer]

def priority(time, priority_expression, lat, long)
	
def smallest_distance(x, numbers):
    index = bisect.bisect_right(numbers,x)
    if index - 1 >= 0:
        distance = abs(x[0] - numbers[index-1][0])
        if index < len(numbers):
            distance = min(distance, abs(x[0] - numbers[index][0]))
    else:
        distance = abs(x[0] - numbers[index][0])
#    print x,index,numbers,distance
    return distance

def get_sound_file_pathnames_times(pathname, remote_host=''):
    command = "find '%s' -name '*.wv' "% (pathname)
    if remote_host:
        command = 'ssh %s %s \|bzip2 -9 -c -|bunzip2 -c -' % (remote_host,command)
    pathnames_times = []
    print command
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
    for filename in process.stdout:
        m = re.search(r'(\d{9}[\d\.]+)', filename)
        if m:
            time = float(m.group(1))
            pathnames_times.append((time, filename.rstrip('\n')))
    process.stdout.close()
    if process.wait() != 0:
        raise RuntimeError, '%s failed' % (command)
    return sorted(pathnames_times)

if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option("-m", "--maximum_bytes", dest="maximum_bytes",type="int", help="stop transfer after BYTES bytes transferred",metavar="BYTES",default=10000000)
    parser.add_option("-f", "--maximum_failures", dest="maximum_failures",help="stop transfer after N failures",metavar="N",default=3)
    parser.add_option("-s", "--station_name", dest="station_name",help="station name",default='peachester')
    parser.add_option("-H", "--remote_host", dest="remote_host",help="remote host",default='root@peachester.homelinux.org')
    parser.add_option("-l", "--local_pathname", dest="local_pathname",help="local directory PATHNAME")
    parser.add_option("-r", "--remote_pathname", dest="remote_pathname",help="remote directory PATHNAME",metavar="PATHNAME")
    parser.add_option("-k", "--keep_source_files", dest="keep_source_files", action="store_true", help="do not remove source files")
    (options, args) = parser.parse_args()
    remote_pathname = (options.remote_pathname or '/var/lib/bowerbird/data/%s' % options.station_name) + '/.'
    local_pathname = options.local_pathname or remote_pathname
    extra_args = '' if options.keep_source_files  else '--remove-source-files'
    print "remote_pathname=%s local_pathname=%s\n" % (remote_pathname, local_pathname)
    remote_files = get_sound_file_pathnames_times(remote_pathname, remote_host=options.remote_host)
    local_files = get_sound_file_pathnames_times(local_pathname)
    total_bytes_transferred = 0
    n_failures = 0
    while total_bytes_transferred < options.maximum_bytes and n_failures < options.maximum_failures :
        files =  find_best_sound_files_to_transfer(remote_files, local_files, how_many_files=10)
#        print 'files chosen: ', files
        if not files:
            break
        command = "ssh '%s' rsync -av '%s' 'bowerbird\@glebe.homelinux.org::bowerbird/%s/' --whole-file --relative %s --password-file /etc/rsyncd.password" % (options.remote_host, "' '".join(files), options.station_name, extra_args)
        print command
        process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        bytes_transferred = 0
        for line in process.stdout:
#            print 'line:', line
            m = re.search(r'sent\s*(\d+)\s*bytes', line)
            if m:
                bytes_transferred = int(m.group(1))
        process.stdout.close()
        if bytes_transferred == 0:
            n_failures += 1
        total_bytes_transferred += bytes_transferred
        print "%d total_bytes_transferred - will stop at %d bytes\n" % (total_bytes_transferred, options.maximum_bytes)

