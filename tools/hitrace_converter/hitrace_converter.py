#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import re
import copy
import optparse
import struct
import os
import stat

import parse_functions

TRACE_REGEX_ASYNC = "\s*(\d+)\s+(.*?)\|\d+\|[SFC]\s+:(.*?)\s+:(.*?)\s+(.*?)\s+\]\d+\[\s+\)(\d+)\s*\(\s+(\d+?)-(.*?)\s+"
TRACE_REGEX_SYNC = "\s*\|\d+\|E\s+:(.*?)\s+:(.*?)\s+(.*?)\s+\]\d+\[\s+\)(\d+)\s*\(\s+(\d+?)-(.*?)\s+"
text_file = ""
binary_file = ""
out_file = ""

CONTENT_TYPE_DEFAULT = 0
CONTENT_TYPE_EVENTS_FORMAT = 1
CONTENT_TYPE_CMDLINES = 2
CONTENT_TYPE_TGIDS = 3
CONTENT_TYPE_CPU_RAW = 4
CONTENT_TYPE_HEADER_PAGE = 30
CONTENT_TYPE_PRINTK_FORMATS = 31
CONTENT_TYPE_KALLSYMS = 32

INT8_DATA_READ_LEN = 1
INT16_DATA_READ_LEN = 2
INT32_DATA_READ_LEN = 4
INT64_DATA_READ_LEN = 8

READ_PAGE_SIZE = 4096

RB_MISSED_FLAGS = (0x1 << 31) | (1 << 30)

BUFFER_TYPE_PADDING = 29
BUFFER_TYPE_TIME_EXTEND = 30
BUFFER_TYPE_TIME_STAMP = 31

events_format = {}
cmd_lines = {}
tgids = {}


TRACE_TXT_HEADER_FORMAT = """# tracer: nop
#
# entries-in-buffer/entries-written: %lu/%lu   #P:%d
#
#                                      _-----=> irqs-off
#                                     / _----=> need-resched
#                                    | / _---=> hardirq/softirq
#                                    || / _--=> preempt-depth
#                                    ||| /     delay
#           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |        |      |   ||||       |         |
"""


def parse_options():
    global text_file
    global binary_file
    global out_file

    usage = "Usage: %prog -t text_file -o out_file or\n%prog -b binary_file -o out_file"
    desc = "Example: %prog -t my_trace_file.htrace -o my_trace_file.systrace"

    parser = optparse.OptionParser(usage=usage, description=desc)
    parser.add_option('-t', '--text_file', dest='text_file',
        help='Name of the text file to be parsed.', metavar='FILE')
    parser.add_option('-b', '--binary_file', dest='binary_file',
        help='Name of the binary file to be parsed.', metavar='FILE')
    parser.add_option('-o', '--out_file', dest='out_file',
        help='File name after successful parsing.', metavar='FILE')

    options, args = parser.parse_args()

    if options.out_file is not None:
        out_file = options.out_file
    else:
        print("Error: out_file must be specified")
        exit(-1)
    if options.text_file is not None:
        text_file = options.text_file
    if options.binary_file is not None:
        binary_file = options.binary_file

    if text_file == '' and binary_file == '':
        print("Error: You must specify a text or binary file")
        exit(-1)
    if text_file != '' and binary_file != '':
        print("Error: Only one parsed file can be specified")
        exit(-1)


def parse_text_trace_file():
    print("start processing text trace file")
    pattern_async = re.compile(TRACE_REGEX_ASYNC)
    pattern_sync = re.compile(TRACE_REGEX_SYNC)
    match_num = 0

    infile_flags = os.O_RDONLY
    infile_mode = stat.S_IRUSR
    infile = os.fdopen(os.open(text_file, infile_flags, infile_mode), "r", encoding="utf-8")
    outfile_flags = os.O_RDWR | os.O_CREAT
    outfile_mode = stat.S_IRUSR | stat.S_IWUSR
    outfile = os.fdopen(os.open(out_file, outfile_flags, outfile_mode), "w+", encoding="utf-8")

    for line in infile:
        reverse_line = line[::-1]
        trace_match_async = pattern_async.match(reverse_line)
        trace_match_sync = pattern_sync.match(reverse_line)
        if trace_match_async:
            line = line.rstrip(' ')
            pos = line.rfind(' ')
            line = "%s%s%s" % (line[:pos], '|', line[pos+1:])
            match_num += 1
        elif trace_match_sync:
            line = "%s\n" % (line.rstrip()[:-1])
            match_num += 1
        outfile.write(line)
    infile.close()
    outfile.close()
    print("total matched and modified lines: ", match_num)


cpu_raw_read_pos = 0
TRACE_HEADER_SIZE = 12


def parse_trace_header(infile):
    trace_header = {}
    trace_header_data = infile.read(TRACE_HEADER_SIZE)
    trace_header_tuple = struct.unpack('HBHL', trace_header_data)
    trace_header["magic_number"] = trace_header_tuple[0]
    trace_header["file_type"] = trace_header_tuple[1]
    trace_header["version_number"] = trace_header_tuple[2]
    trace_header["reserved"] = trace_header_tuple[3]
    return trace_header


def parse_page_header(data):
    global cpu_raw_read_pos
    page_header = {}

    struct_page_header = struct.unpack('QQB', \
    data[cpu_raw_read_pos:cpu_raw_read_pos + INT64_DATA_READ_LEN * 2 + INT8_DATA_READ_LEN])
    cpu_raw_read_pos += INT64_DATA_READ_LEN * 2 + INT8_DATA_READ_LEN
    page_header["time_stamp"] = struct_page_header[0]

    page_header["length"] = struct_page_header[1]
    page_header["core_id"] = struct_page_header[2]

    return page_header


def parse_event_header(data):
    global cpu_raw_read_pos
    event_header = {}

    struct_event_header = struct.unpack('LH', \
    data[cpu_raw_read_pos:cpu_raw_read_pos + INT32_DATA_READ_LEN + INT16_DATA_READ_LEN])
    event_header["time_stamp_offset"] = struct_event_header[0]
    event_header["size"] = struct_event_header[1]

    cpu_raw_read_pos += INT32_DATA_READ_LEN + INT16_DATA_READ_LEN

    return event_header


TRACE_FLAG_IRQS_OFF = 0x01
TRACE_FLAG_IRQS_NOSUPPORT = 0x02
TRACE_FLAG_NEED_RESCHED = 0x04
TRACE_FLAG_HARDIRQ = 0x08
TRACE_FLAG_SOFTIRQ = 0x10
TRACE_FLAG_PREEMPT_RESCHED = 0x20
TRACE_FLAG_NMI = 0x40


def trace_flags_to_str(flags, preempt_count):
    result = ""
    irqs_off = '.'
    if flags & TRACE_FLAG_IRQS_OFF != 0:
        irqs_off = 'd'
    elif flags & TRACE_FLAG_IRQS_NOSUPPORT != 0:
        irqs_off = 'X'
    result += irqs_off

    need_resched = '.'
    is_need_resched = flags & TRACE_FLAG_NEED_RESCHED
    is_preempt_resched = flags & TRACE_FLAG_PREEMPT_RESCHED
    if is_need_resched != 0 and is_preempt_resched != 0:
        need_resched = 'N'
    elif is_need_resched != 0:
        need_resched = 'n'
    elif is_preempt_resched != 0:
        need_resched = 'p'
    result += need_resched

    nmi_flag = flags & TRACE_FLAG_NMI
    hard_irq = flags & TRACE_FLAG_HARDIRQ
    soft_irq = flags & TRACE_FLAG_SOFTIRQ
    irq_char = '.'
    if nmi_flag != 0 and hard_irq != 0:
        irq_char = 'Z'
    elif nmi_flag != 0:
        irq_char = 'z'
    elif hard_irq != 0 and soft_irq != 0:
        irq_char = 'H'
    elif hard_irq != 0:
        irq_char = 'h'
    elif soft_irq != 0:
        irq_char = 's'
    result += irq_char

    if preempt_count != 0:
        result += "0123456789abcdef"[preempt_count & 0x0F]
    else:
        result += "."

    return result


COMM_STR_MAX = 16
PID_STR_MAX = 6
TGID_STR_MAX = 5
CPU_STR_MAX = 3
TS_SECS_MIN = 5
TS_MICRO_SECS = 6


def generate_one_event_str(data, cpu_id, time_stamp, one_event):
    pid = int.from_bytes(one_event["fields"]["common_pid"], byteorder='little')
    event_str = ""

    cmd_line = cmd_lines.get(pid, "")
    if pid == 0:
        event_str += "<idle>"
    elif cmd_line != "":
        event_str += cmd_line
    else:
        event_str += "<...>"
    event_str = event_str.rjust(COMM_STR_MAX)
    event_str += "-"

    event_str += str(pid).ljust(PID_STR_MAX)

    tgid = tgids.get(pid, "")
    if tgid != "":
        event_str += "(" + tgid.rjust(TGID_STR_MAX) + ")"
    else:
        event_str += "(-----)"

    event_str += " [" + str(cpu_id).zfill(CPU_STR_MAX) + "] "

    flags = int.from_bytes(one_event["fields"]["common_flags"], byteorder='little')
    preempt_count = int.from_bytes(one_event["fields"]["common_preempt_count"], byteorder='little')
    if flags | preempt_count != 0:
        event_str += trace_flags_to_str(flags, preempt_count) + " "
    else:
        event_str += ".... "

    if time_stamp % 1000 >= 500:
        time_stamp_str = str((time_stamp // 1000) + 1)
    else:
        time_stamp_str = str(time_stamp // 1000)
    ts_secs = time_stamp_str[:-6].rjust(TS_SECS_MIN)
    ts_micro_secs = time_stamp_str[-6:]
    event_str += ts_secs + "." + ts_micro_secs + ": "

    parse_result = parse_functions.parse(one_event["print_fmt"], data, one_event)
    if parse_result is None:
        print("Error: function parse_" + str(one_event["name"]) + " not found")
    else:
        event_str += str(one_event["name"]) + ": " + parse_result

    return event_str


def parse_one_event(data, event_id, cpu_id, time_stamp):
    event_format = events_format.get(event_id, "")
    if event_format == "":
        return ""

    fields = event_format["fields"]
    one_event = {}
    one_event["id"] = event_id
    one_event["name"] = event_format["name"]
    one_event["print_fmt"] = event_format["print_fmt"]
    one_event["fields"] = {}
    for field in fields:
        offset = field["offset"]
        size = field["size"]
        one_event["fields"][field["name"]] = data[offset:offset + size]

    return generate_one_event_str(data, cpu_id, time_stamp, one_event)


RMQ_ENTRY_ALIGN_MASK = 3


def parse_cpu_raw_one_page(data, result):
    global cpu_raw_read_pos
    end_pos = cpu_raw_read_pos + READ_PAGE_SIZE
    page_header = parse_page_header(data)

    while cpu_raw_read_pos < end_pos:
        event_header = parse_event_header(data)
        if event_header.get("size", 0) == 0:
            break

        time_stamp = page_header.get("time_stamp", 0) + event_header.get("time_stamp_offset", 0)
        event_id = struct.unpack('H', data[cpu_raw_read_pos:cpu_raw_read_pos + INT16_DATA_READ_LEN])[0]

        one_event_data = data[cpu_raw_read_pos:cpu_raw_read_pos + event_header.get("size", 0)]
        one_event_result = parse_one_event(one_event_data, event_id, page_header.get("core_id", 0), time_stamp)
        if one_event_result != "":
            result.append([time_stamp, one_event_result])

        evt_size = ((event_header.get("size", 0) + RMQ_ENTRY_ALIGN_MASK) & (~RMQ_ENTRY_ALIGN_MASK))
        cpu_raw_read_pos += evt_size
    cpu_raw_read_pos = end_pos


def parse_cpu_raw(data, data_len, result):
    global cpu_raw_read_pos
    cpu_raw_read_pos = 0

    while cpu_raw_read_pos < data_len:
        parse_cpu_raw_one_page(data, result)


def parse_events_format_field(field_line):
    field_info = field_line.split(";")
    field_info[0] = field_info[0].lstrip()
    field_info[1] = field_info[1].lstrip()
    field_info[2] = field_info[2].lstrip()
    field_info[3] = field_info[3].lstrip()

    field = {}
    type_name_pos = field_info[0].rfind(" ")
    field["type"] = field_info[0][len("field:"):type_name_pos]
    field["name"] = field_info[0][type_name_pos + 1:]
    field["offset"] = int(field_info[1][len("offset:"):])
    field["size"] = int(field_info[2][len("size:"):])
    field["signed"] = field_info[3][len("signed:"):]

    return field


def parse_events_format(data):
    name_line_prefix = "name: "
    id_line_prefix = "ID: "
    field_line_prefix = "field:"
    print_fmt_line_prefix = "print fmt: "

    events_format_lines = data.decode('utf-8').split("\n")
    event_format = {}
    event_format["fields"] = []
    for line in events_format_lines:
        line = line.lstrip()
        if line.startswith(name_line_prefix):
            event_format["name"] = line[len(name_line_prefix):]
        elif line.startswith(id_line_prefix):
            event_format["id"] = int(line[len(id_line_prefix):])
        elif line.startswith(field_line_prefix):
            event_format["fields"].append(parse_events_format_field(line))
        elif line.startswith(print_fmt_line_prefix):
            event_format["print_fmt"] = line[len(print_fmt_line_prefix):]
            events_format[event_format["id"]] = copy.deepcopy(event_format)
            event_format["fields"].clear()


def parse_cmdlines(data):
    cmd_lines_list = data.decode('utf-8').split("\n")
    for cmd_line in cmd_lines_list:
        pos = cmd_line.find(" ")
        if pos != -1:
            cmd_lines[int(cmd_line[:pos])] = cmd_line[pos + 1:]


def parse_tgids(data):
    tgids_lines_list = data.decode('utf-8').split("\n")
    for tgids_line in tgids_lines_list:
        pos = tgids_line.find(" ")
        if pos != -1:
            tgids[int(tgids_line[:pos])] = tgids_line[pos + 1:]


def parse_header_page(data):
    print("in parse_header_page")


def parse_printk_formats(data):
    print("in parse_printk_formats")


def parse_kallsyms(data):
    print("in parse_kallsyms")


def parse_trace_base_data(infile, file_size):
    while infile.tell() < file_size:
        data_type = struct.unpack('L', infile.read(INT32_DATA_READ_LEN))[0]
        data_len = struct.unpack('L', infile.read(INT32_DATA_READ_LEN))[0]
        data = infile.read(data_len)
        if data_type == CONTENT_TYPE_HEADER_PAGE:
            parse_header_page(data)
        elif data_type == CONTENT_TYPE_CMDLINES:
            parse_cmdlines(data)
        elif data_type == CONTENT_TYPE_TGIDS:
            parse_tgids(data)
        elif data_type == CONTENT_TYPE_EVENTS_FORMAT:
            parse_events_format(data)
        elif data_type == CONTENT_TYPE_PRINTK_FORMATS:
            parse_printk_formats(data)
        elif data_type == CONTENT_TYPE_KALLSYMS:
            parse_kallsyms(data)


def parse_trace_events_data(infile, file_size, cpu_nums, result):
    while infile.tell() < file_size:
        data_type = struct.unpack('L', infile.read(INT32_DATA_READ_LEN))[0]
        data_len = struct.unpack('L', infile.read(INT32_DATA_READ_LEN))[0]
        data = infile.read(data_len)

        if data_type >= CONTENT_TYPE_CPU_RAW and data_type < CONTENT_TYPE_CPU_RAW + cpu_nums:
            parse_cpu_raw(data, data_len, result)


def parse_binary_trace_file():
    infile_flags = os.O_RDONLY | os.O_BINARY
    infile_mode = stat.S_IRUSR
    infile = os.fdopen(os.open(binary_file, infile_flags, infile_mode), 'rb')

    outfile_flags = os.O_RDWR | os.O_CREAT
    outfile_mode = stat.S_IRUSR | stat.S_IWUSR
    outfile = os.fdopen(os.open(out_file, outfile_flags, outfile_mode), 'w')

    trace_header = parse_trace_header(infile)
    cpu_nums = (trace_header.get("reserved", 0) >> 1) & 0xf

    outfile.write(TRACE_TXT_HEADER_FORMAT)
    trace_file_size = os.path.getsize(binary_file)
    parse_trace_base_data(infile, trace_file_size)
    infile.seek(TRACE_HEADER_SIZE)

    result = []
    parse_trace_events_data(infile, trace_file_size, cpu_nums, result)
    result = sorted(result, key=lambda x: x[0])
    for line in result:
        outfile.write("{}\n".format(line[1]))

    outfile.close()
    infile.close()


def main():
    parse_options()

    if text_file != '':
        parse_text_trace_file()
    else:
        parse_binary_trace_file()


if __name__ == '__main__':
    main()