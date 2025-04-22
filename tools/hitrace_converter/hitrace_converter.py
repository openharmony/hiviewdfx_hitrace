#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025 Huawei Device Co., Ltd.
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

from abc import ABCMeta, abstractmethod
from enum import IntEnum, unique
from typing import List, Any
import optparse
import os
import stat
import struct
import re

import parse_functions

TRACE_REGEX_ASYNC = "\s*(\d+)\s+(.*?)\|\d+\|[SFC]\s+:(.*?)\s+:(.*?)\s+(.*?)\s+\]\d+\[\s+\)(\d+)\s*\(\s+(\d+?)-(.*?)\s+"
TRACE_REGEX_SYNC = "\s*\|\d+\|E\s+:(.*?)\s+:(.*?)\s+(.*?)\s+\]\d+\[\s+\)(\d+)\s*\(\s+(\d+?)-(.*?)\s+"
text_file = ""
binary_file = ""
out_file = ""
file_dir = ""

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


def parse_options() -> None:
    global text_file
    global binary_file
    global out_file
    global file_dir

    usage = "Usage: %prog -t text_file -o out_file or\n%prog -b binary_file -o out_file"
    desc = "Example: %prog -t my_trace_file.htrace -o my_trace_file.systrace"

    parser = optparse.OptionParser(usage=usage, description=desc)
    parser.add_option('-t', '--text_file', dest='text_file',
        help='Name of the text file to be parsed.', metavar='FILE')
    parser.add_option('-b', '--binary_file', dest='binary_file',
        help='Name of the binary file to be parsed.', metavar='FILE')
    parser.add_option('-o', '--out_file', dest='out_file',
        help='File name after successful parsing.', metavar='FILE')
    parser.add_option('-d', '--file_dir', dest='file_dir',
        help='Folder to be parsed.', metavar='FILE')

    options, args = parser.parse_args()

    if options.file_dir is None:
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
    else:
        folder = os.path.exists(options.file_dir)
        if not folder:
            print("Error: file_dir does not exist")
            exit(-1)
        else:
            file_dir = options.file_dir


def parse_text_trace_file() -> None:
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
            line = "%s%s%s" % (line[:pos], '|', line[pos + 1:])
            match_num += 1
        elif trace_match_sync:
            line = "%s\n" % (line.rstrip()[:-1])
            match_num += 1
        outfile.write(line)
    infile.close()
    outfile.close()
    print("total matched and modified lines: ", match_num)


class DataType:
    @staticmethod
    def get_data_bytes(data_types: str) -> int:
        return struct.calcsize(data_types)


@unique
class FieldType(IntEnum):
    # 对HiTrace文件中逻辑划分的域
    TRACE_FILE_HEADER = 1001
    TRACE_EVENT_PAGE_HEADER = 1002
    TRACE_EVENT_HEADER = 1003
    TRACE_EVENT_CONTENT = 1004

    # HiTrace文件中定义的段类型取值
    SEGMENT_SEGMENTS = 0
    SEGMENT_EVENTS_FORMAT = 1
    SEGMENT_CMDLINES = 2
    SEGMENT_TGIDS = 3
    SEGMENT_RAW_TRACE = 4
    SEGMENT_HEADER_PAGE = 30
    SEGMENT_PRINTK_FORMATS = 31
    SEGMENT_KALLSYMS = 32
    SEGMENT_UNSUPPORT = -1
    pass


class Field:
    """
    功能描述: field为一个复合的结构，该结构包含多少item，每个item都有自已的类型
    """
    def __init__(self, vtype: int, size: int, vformat: str, item_types: List) -> None:
        self.field_type = vtype
        self.field_size = size
        self.format = vformat
        self.item_types = item_types
        pass


class TraceParseContext:
    """
    功能描述: 解析过程的中间数据，需要缓存到上下文中，解决段与段间的数据依赖问题
    """
    CONTEXT_CPU_NUM = 1
    CONTEXT_CMD_LINES = 2
    CONTEXT_EVENT_SIZE = 3
    CONTEXT_CORE_ID = 5
    CONTEXT_TIMESTAMP = 6
    CONTEXT_TIMESTAMP_OFFSET = 7
    CONTEXT_TID_GROUPS = 8
    CONTEXT_EVENT_FORMAT = 9

    def __init__(self) -> None:
        self.values = {}
        self.default_values = {
            TraceParseContext.CONTEXT_CPU_NUM: 1,
            TraceParseContext.CONTEXT_CMD_LINES: {},
            TraceParseContext.CONTEXT_EVENT_SIZE: 0,
            TraceParseContext.CONTEXT_CORE_ID: 0,
            TraceParseContext.CONTEXT_TIMESTAMP: 0,
            TraceParseContext.CONTEXT_TIMESTAMP_OFFSET: 0,
            TraceParseContext.CONTEXT_TID_GROUPS: {},
            TraceParseContext.CONTEXT_EVENT_FORMAT: {},
        }
        pass

    def set_param(self, key: int, value: Any) -> None:
        self.values[key] = value
        pass

    def get_param(self, key: int) -> Any:
        value = self.values.get(key)
        if value is None:
            return self.default_values.get(key)
        return value


class TraceViewerInterface(metaclass=ABCMeta):
    """
    功能描述: 声明解析HiTrace结果的接口
    """
    @abstractmethod
    def append_trace_event(self, trace_event: List) -> None:
        pass

    @abstractmethod
    def calculate(self, context: TraceParseContext) -> None:
        pass


class TraceFileParserInterface(metaclass=ABCMeta):
    """
    功能描述: 声明解析HiTrace文件的接口
    """
    @abstractmethod
    def parse_simple_field(self, field: Field) -> tuple:
        return ()

    @abstractmethod
    def parse_event_format(self, data: List) -> dict:
        return {}

    @abstractmethod
    def parse_cmd_lines(self, data: List) -> dict:
        return {}

    @abstractmethod
    def parse_tid_groups(self, data: List) -> dict:
        return {}

    @abstractmethod
    def get_context(self) -> TraceParseContext:
        return None

    @abstractmethod
    def get_viewer(self) -> TraceViewerInterface:
        return None

    @abstractmethod
    def get_segment_data(self, segment_size) -> List:
        return None


class OperatorInterface(metaclass=ABCMeta):
    @abstractmethod
    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        return True


class FieldOperator(Field, OperatorInterface):
    def __init__(self, vtype, size, vformat, item_types) -> None:
        Field.__init__(self, vtype, size, vformat, item_types)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        return True


class SegmentOperator(Field, OperatorInterface):
    def __init__(self, vtype) -> None:
        Field.__init__(self, vtype, -1, "", [])
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        return True


class FileHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的头部格式
    """
    # 描述HiTrace文件头的pack格式
    FORMAT = "HBHL"

    # HiTrace文件头包含的字段
    ITEM_MAGIC_NUMBER = 0
    ITEM_FILE_TYPE = 1
    ITEM_VERSION_NUMBER = 2
    ITEM_RESERVED = 3

    def __init__(self, item_types=None) -> None:
        item_types = item_types or []
        super().__init__(
            FieldType.TRACE_FILE_HEADER,
            DataType.get_data_bytes(FileHeader.FORMAT),
            FileHeader.FORMAT,
            item_types
        )

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        data = parser.get_segment_data(self.field_size)
        values = parser.parse_simple_field(self.format, data)
        if len(values) == 0:
            return False
        cpu_num = (values[FileHeader.ITEM_RESERVED] >> 1) & 0b00011111
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_CPU_NUM, cpu_num)

        version = values[FileHeader.ITEM_VERSION_NUMBER]
        file_type = values[FileHeader.ITEM_FILE_TYPE]
        print("version is %d, file type is %d, cpu number %d" % (version, file_type, cpu_num))
        return True


class PageHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的第个 raw event 页头部格式
    """
    # 描述HiTrace文件的page header的pack格式
    FORMAT = "QQB"

    # HiTrace文件的page header包含的字段，每一页是同一个CPU核产生的数据，每一页都是固定大小4096字节
    # page内trace event的时间基准点
    ITEM_TIMESTAMP = 0
    ITEM_LENGTH = 1
    # page所属的CPU核
    ITEM_CORE_ID = 2

    def __init__(self, item_types: List) -> None:
        super().__init__(
            FieldType.TRACE_EVENT_PAGE_HEADER,
            DataType.get_data_bytes(PageHeader.FORMAT),
            PageHeader.FORMAT,
            item_types
        )

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        value = parser.parse_simple_field(self.format, segment)
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_TIMESTAMP, value[PageHeader.ITEM_TIMESTAMP])
        context.set_param(TraceParseContext.CONTEXT_CORE_ID, value[PageHeader.ITEM_CORE_ID])
        return True
    pass


class TraceEventHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的trace event头部格式
    """
    # 描述HiTrace文件trace event的pack格式
    FORMAT = "LH"
    RMQ_ENTRY_ALIGN_MASK = 3

    # HiTrace文件的trace event header包含的字段，事件的打点时间是基于page header的时间做偏移可计算出来
    ITEM_TIMESTAMP_OFFSET = 0
    # trace event的大小，实际占用内存的大小还需要考虑内存对齐的因素
    ITEM_EVENT_SIZE = 1

    def __init__(self, item_types: List) -> None:
        super().__init__(
            FieldType.TRACE_EVENT_HEADER,
            DataType.get_data_bytes(TraceEventHeader.FORMAT),
            TraceEventHeader.FORMAT,
            item_types
        )

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        value = parser.parse_simple_field(self.format, segment)
        if len(value) == 0:
            return False
        event_size = value[TraceEventHeader.ITEM_EVENT_SIZE]
        align_event_size = ((event_size + TraceEventHeader.RMQ_ENTRY_ALIGN_MASK) & (~TraceEventHeader.RMQ_ENTRY_ALIGN_MASK))

        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_EVENT_SIZE, (event_size, align_event_size))
        context.set_param(TraceParseContext.CONTEXT_TIMESTAMP_OFFSET, value[TraceEventHeader.ITEM_TIMESTAMP_OFFSET])
        return True
    pass


class TraceEventContent(SegmentOperator):
    """
    功能描述: 声明HiTrace文件的trace event的内容格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.TRACE_EVENT_CONTENT)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        context = parser.get_context()
        timestamp = context.get_param(TraceParseContext.CONTEXT_TIMESTAMP)
        timestamp = timestamp + context.get_param(TraceParseContext.CONTEXT_TIMESTAMP_OFFSET)
        core_id = context.get_param(TraceParseContext.CONTEXT_CORE_ID)

        vformat = 'H'
        data_bytes = DataType.get_data_bytes(vformat)
        event_id = parser.parse_simple_field(vformat, segment[0:data_bytes])
        if len(event_id) == 0:
            return False
        trace_event = [timestamp, core_id, event_id[0], segment]
        viewer = parser.get_viewer()
        viewer.append_trace_event(trace_event)
        return True
    pass


class TraceEventWrapper(OperatorInterface):
    """
    功能描述: 声明HiTrace文件的1条trace event的格式
    """
    def __init__(self, event_header: TraceEventHeader, event_content: TraceEventContent) -> None:
        self.event_header = event_header
        self.event_content = event_content
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        cur_post = 0
        while cur_post < len(segment):
            (cur_post, data) = self.next_event_header(cur_post, segment)
            if self.event_header.accept(parser, data) is False:
                break
            (cur_post, data) = self.next_event_context(cur_post, segment, parser)
            if self.event_content.accept(parser, data) is False:
                break
        return True

    def next_event_context(self, cur_post: int, segment: List, parser: TraceFileParserInterface) -> tuple:
        context = parser.get_context()
        (event_size, align_event_size) = context.get_param(TraceParseContext.CONTEXT_EVENT_SIZE)
        data = segment[cur_post: cur_post + event_size]
        next_post = cur_post + align_event_size
        return (next_post, data)

    def next_event_header(self, cur_post: int, segment: List) -> tuple:
        data = segment[cur_post: cur_post + self.event_header.field_size]
        next_post = cur_post + self.event_header.field_size
        return (next_post, data)
    pass


class PageWrapper(OperatorInterface):
    """
    功能描述: 声明HiTrace文件的1个page的格式
    """
    # trace event的page是固定大小, trace event的内容大小，应该是4096的整数倍，trace event需要按页进行解析
    TRACE_PAGE_SIZE = 4096

    def __init__(self, page_header: PageHeader, trace_event: TraceEventWrapper) -> None:
        self.page_header = page_header
        self.trace_event = trace_event
        pass

    def next(self, cur_post: int, segment: List) -> tuple:
        if segment is None:
            return (-1, None)

        if (cur_post + PageWrapper.TRACE_PAGE_SIZE) > len(segment):
            return (-1, None)

        data = segment[cur_post: cur_post + PageWrapper.TRACE_PAGE_SIZE]
        next_post = cur_post + PageWrapper.TRACE_PAGE_SIZE
        return (next_post, data)

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        cur_post = 0
        while True:
            (cur_post, data) = self.next(cur_post, segment)
            if data is None:
                break

            self.page_header.accept(parser, data[0: self.page_header.field_size])
            self.trace_event.accept(parser, data[self.page_header.field_size:])
        return True
    pass


class RawTraceSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件trace_pipe_raw内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_RAW_TRACE)
        self.field = PageWrapper(
            PageHeader([
                PageHeader.ITEM_TIMESTAMP,
                PageHeader.ITEM_LENGTH,
                PageHeader.ITEM_CORE_ID,
            ]),
            TraceEventWrapper(
                TraceEventHeader([
                    TraceEventHeader.ITEM_TIMESTAMP_OFFSET,
                    TraceEventHeader.ITEM_EVENT_SIZE
                ]),
                TraceEventContent(),
            )
        )
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        self.field.accept(parser, segment)
        return True


class EventFormatSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件event/format内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_EVENTS_FORMAT)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        events_format = parser.parse_event_format(segment)
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_EVENT_FORMAT, events_format)
        return True


class CmdLinesSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件/sys/kernel/tracing/saved_cmdlines内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_CMDLINES)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        cmd_lines = parser.parse_cmd_lines(segment)
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_CMD_LINES, cmd_lines)
        return True


class TidGroupsSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件/sys/kernel/tracing/saved_tgids内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_TGIDS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        tid_groups = parser.parse_tid_groups(segment)
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_TID_GROUPS, tid_groups)
        return True


class PrintkFormatSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件/sys/kernel/tracing/printk_formats内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_PRINTK_FORMATS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        print("in parse_printk_formats")
        return True


class KallSymsSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件Kall Sysms内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_KALLSYMS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        print("in parse_kallsyms")
        return True


class HeaderPageSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件header page内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_HEADER_PAGE)
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        print("in parse_header_page")
        return True


class UnSupportSegment(FieldOperator):
    """
    功能描述: 声明HiTrace文件还不支持解析的段
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_UNSUPPORT, -1, "", [])
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        print("unsupport segment")
        return True


class SegmentWrapper(FieldOperator):
    """
    功能描述: 声明HiTrace文件包含所有段的格式
    """
    # 描述段的pack的格式
    FORMAT = "II"

    ITEM_SEGMENT_TYPE = 0
    ITEM_SEGMENT_SIZE = 1

    def __init__(self, fields: List) -> None:
        super().__init__(
            FieldType.SEGMENT_SEGMENTS,
            DataType.get_data_bytes(SegmentWrapper.FORMAT),
            SegmentWrapper.FORMAT,
            [
                SegmentWrapper.ITEM_SEGMENT_TYPE,
                SegmentWrapper.ITEM_SEGMENT_SIZE,
            ]
        )
        self.fields = fields
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        segment = segment or []
        while True:
            data = parser.get_segment_data(self.field_size)
            values = parser.parse_simple_field(self.format, data)
            if len(values) == 0:
                break
            segment_type = values[SegmentWrapper.ITEM_SEGMENT_TYPE]
            segment_size = values[SegmentWrapper.ITEM_SEGMENT_SIZE]
            conext = parser.get_context()

            segment = self.get_segment(segment_type, conext.get_param(TraceParseContext.CONTEXT_CPU_NUM))
            segment_data = parser.get_segment_data(segment_size)
            segment.accept(parser, segment_data)
            pass
        return True

    def get_segment(self, segment_type: int, cpu_num: int) -> FieldOperator:
        for field in self.fields:
            if field.field_type == segment_type:
                return field

            if field.field_type < FieldType.SEGMENT_RAW_TRACE:
                continue

            if field.field_type > FieldType.SEGMENT_RAW_TRACE + cpu_num:
                continue
            return field
        return UnSupportSegment()


class TraceFile:
    """
    功能描述: 读取hitrace的二进制文件
    """
    def __init__(self, name: str) -> None:
        self.name = name
        flags = os.O_RDONLY | os.O_BINARY
        mode = stat.S_IRUSR
        self.file = os.fdopen(os.open(name, flags, mode), 'rb')
        self.size = os.path.getsize(name)
        self.cur_post = 0
        pass

    def read_data(self, block_size: int) -> List:
        """
        功能描述: 从文件当前位置读取blockSize字节的内容
        参数: 读取字节数
        返回值: 文件内容
        """
        if (self.cur_post + block_size) > self.size:
            return None
        self.cur_post = self.cur_post + block_size
        return self.file.read(block_size)


class TraceFileFormat(OperatorInterface):
    """
    功能描述: 声明整个HiTrace文件的二进制格式
    """
    def __init__(self) -> None:
        self.fields = [
            FileHeader([
                FileHeader.ITEM_MAGIC_NUMBER,
                FileHeader.ITEM_FILE_TYPE,
                FileHeader.ITEM_VERSION_NUMBER,
                FileHeader.ITEM_RESERVED
            ]),
            SegmentWrapper([
                CmdLinesSegment(),
                TidGroupsSegment(),
                EventFormatSegment(),
                RawTraceSegment(),
                PrintkFormatSegment(),
                KallSymsSegment(),
                HeaderPageSegment(),
            ])
        ]
        pass

    def accept(self, parser: TraceFileParserInterface, segment=None) -> bool:
        for field in self.fields:
            if field.accept(parser) is False:
                return False
        return True


class Viewer:
    @abstractmethod
    def set_context(self, context: TraceParseContext) -> None:
        pass

    @abstractmethod
    def calculate(self, timestamp: int, core_id: int, event_id: int, segment: List) -> None:
        pass

    @abstractmethod
    def show(self) -> None:
        pass


class SysTraceViewer(Viewer):
    COMM_STR_MAX = 16
    PID_STR_MAX = 6
    TGID_STR_MAX = 5
    CPU_STR_MAX = 3
    TS_SECS_MIN = 5
    TS_MICRO_SECS = 6

    TRACE_FLAG_IRQS_OFF = 0x01
    TRACE_FLAG_IRQS_NOSUPPORT = 0x02
    TRACE_FLAG_NEED_RESCHED = 0x04
    TRACE_FLAG_HARDIRQ = 0x08
    TRACE_FLAG_SOFTIRQ = 0x10
    TRACE_FLAG_PREEMPT_RESCHED = 0x20
    TRACE_FLAG_NMI = 0x40

    def __init__(self, output_file: str):
        self.out_file = output_file
        self.format_miss_cnt = 0
        self.format_miss_set = set()
        self.trace_event_count_dict = {} # trace event count dict
        self.trace_event_mem_dict = {} # trace event mem dict
        self.get_not_found_format = set()
        self.systrace = []
        pass

    def set_context(self, context: TraceParseContext) -> None:
        self.context = context
        self.events_format = context.get_param(TraceParseContext.CONTEXT_EVENT_FORMAT)
        self.cmd_lines = context.get_param(TraceParseContext.CONTEXT_CMD_LINES)
        self.tgids = context.get_param(TraceParseContext.CONTEXT_TID_GROUPS)
        pass

    def calculate(self, timestamp: int, core_id: int, event_id: int, segment: List, context: TraceParseContext) -> None:
        if event_id in self.trace_event_count_dict:
            self.trace_event_count_dict[event_id] += 1
        else:
            self.trace_event_count_dict[event_id] = 1

        if event_id in self.trace_event_mem_dict:
            self.trace_event_mem_dict[event_id] += len(segment)
        else:
            self.trace_event_mem_dict[event_id] = len(segment)

        event_format = self.events_format.get(event_id, "")
        if event_format == "":
            # current event format is not found in trace file format data.
            self.format_miss_cnt = self.format_miss_cnt + 1
            self.format_miss_set.add(event_id)
            return

        fields = event_format["fields"]
        one_event = {}
        one_event["id"] = event_id
        one_event["name"] = event_format["name"]
        one_event["print_fmt"] = event_format["print_fmt"]
        one_event["fields"] = {}
        for field in fields:
            offset = field["offset"]
            size = field["size"]
            one_event["fields"][field["name"]] = segment[offset:offset + size]

        systrace = self.generate_one_event_str(segment, core_id, timestamp, one_event)
        self.systrace.append([timestamp, systrace])
        pass

    def generate_one_event_str(self, data: List, cpu_id: int, time_stamp: int, one_event: dict) -> str:
        pid = int.from_bytes(one_event["fields"]["common_pid"], byteorder='little')
        event_str = ""

        cmd_line = self.cmd_lines.get(pid, "")
        if pid == 0:
            event_str += "<idle>"
        elif cmd_line != "":
            event_str += cmd_line
        else:
            event_str += "<...>"
        event_str = event_str.rjust(SysTraceViewer.COMM_STR_MAX)
        event_str += "-"

        event_str += str(pid).ljust(SysTraceViewer.PID_STR_MAX)

        tgid = self.tgids.get(pid)
        if tgid is not None:
            tgid = "%d" % tgid
            event_str += "(" + tgid.rjust(SysTraceViewer.TGID_STR_MAX) + ")"
        else:
            event_str += "(-----)"

        event_str += " [" + str(cpu_id).zfill(SysTraceViewer.CPU_STR_MAX) + "] "

        flags = int.from_bytes(one_event["fields"]["common_flags"], byteorder='little')
        preempt_count = int.from_bytes(one_event["fields"]["common_preempt_count"], byteorder='little')
        if flags | preempt_count != 0:
            event_str += self.trace_flags_to_str(flags, preempt_count) + " "
        else:
            event_str += ".... "

        if time_stamp % 1000 >= 500:
            time_stamp_str = str((time_stamp // 1000) + 1)
        else:
            time_stamp_str = str(time_stamp // 1000)
        ts_secs = time_stamp_str[:-6].rjust(SysTraceViewer.TS_SECS_MIN)
        ts_micro_secs = time_stamp_str[-6:]
        event_str += ts_secs + "." + ts_micro_secs + ": "

        parse_result = parse_functions.parse(one_event["print_fmt"], data, one_event)
        if parse_result is None:
            self.get_not_found_format.add(str(one_event["name"]))
        else:
            event_str += str(one_event["name"]) + ": " + parse_result

        return event_str

    def trace_flags_to_str(self, flags: int, preempt_count: int) -> str:
        result = ""
        irqs_off = '.'
        if flags & SysTraceViewer.TRACE_FLAG_IRQS_OFF != 0:
            irqs_off = 'd'
        elif flags & SysTraceViewer.TRACE_FLAG_IRQS_NOSUPPORT != 0:
            irqs_off = 'X'
        result += irqs_off

        need_resched = '.'
        is_need_resched = flags & SysTraceViewer.TRACE_FLAG_NEED_RESCHED
        is_preempt_resched = flags & SysTraceViewer.TRACE_FLAG_PREEMPT_RESCHED
        if is_need_resched != 0 and is_preempt_resched != 0:
            need_resched = 'N'
        elif is_need_resched != 0:
            need_resched = 'n'
        elif is_preempt_resched != 0:
            need_resched = 'p'
        result += need_resched

        nmi_flag = flags & SysTraceViewer.TRACE_FLAG_NMI
        hard_irq = flags & SysTraceViewer.TRACE_FLAG_HARDIRQ
        soft_irq = flags & SysTraceViewer.TRACE_FLAG_SOFTIRQ
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

    def show(self) -> None:
        self.save_systrace()
        self.show_stat()
        pass

    def save_systrace(self) -> None:
        outfile_flags = os.O_RDWR | os.O_CREAT
        outfile_mode = stat.S_IRUSR | stat.S_IWUSR

        result = sorted(self.systrace, key=lambda x: x[0])
        outfile = os.fdopen(os.open(self.out_file, outfile_flags, outfile_mode), 'w', encoding="utf-8")
        outfile.write(TRACE_TXT_HEADER_FORMAT)
        for line in result:
            outfile.write("{}\n".format(line[1]))
        outfile.close()

    def show_stat(self) -> None:
        for name in self.get_not_found_format:
            print("Error: function parse_%s not found" % name)
        print("Trace format miss count: %d" % self.format_miss_cnt)
        print("Trace format id missed set:")
        print(self.format_miss_set)

        count_total = sum(self.trace_event_count_dict.values())
        mem_total = sum(self.trace_event_mem_dict.values()) / 1024 # KB
        print(f"Trace counter: total count({count_total}), total mem({mem_total:.3f}KB)")
        for format_id, count in self.trace_event_count_dict.items():
            count_percentage = count / count_total * 100
            mem = self.trace_event_mem_dict.get(format_id, 0) / 1024
            mem_percentage = mem / mem_total * 100
            print(f"ID {format_id}: count={count}, count percentage={count_percentage:.5f}%, mem={mem:.3f}KB, mem percentage={mem_percentage:.5f}%")


class TraceViewer(TraceViewerInterface):
    def __init__(self, viewers: List) -> None:
        self.trace_events = []
        self.viewers = viewers
        pass

    def append_trace_event(self, trace_event: List) -> None:
        self.trace_events.append(trace_event)
        pass

    def calculate(self, context: TraceParseContext):
        for viewer in self.viewers:
            viewer.set_context(context)

        for timestamp, core_id, event_id, segment in self.trace_events:
            for viewer in self.viewers:
                viewer.calculate(timestamp, core_id, event_id, segment, context)

        for viewer in self.viewers:
            viewer.show()
        pass


class TraceFileParser(TraceFileParserInterface):
    def __init__(self, file: TraceFile, vformat: TraceFileFormat, viewer: TraceViewer, context: TraceParseContext) -> None:
        self.trace_file = file
        self.trace_format = vformat
        self.trace_viewer = viewer
        self.context = context
        pass

    def parse_simple_field(self, vformat: str, data: List) -> tuple:
        if data is None:
            return {}
        if DataType.get_data_bytes(vformat) != len(data):
            return {}
        unpack_value = struct.unpack(vformat, data)
        return unpack_value

    def parse_event_format(self, data: List) -> dict:
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

        events_format = {}
        name_line_prefix = "name: "
        id_line_prefix = "ID: "
        field_line_prefix = "field:"
        print_fmt_line_prefix = "print fmt: "

        events_format_lines = data.decode('utf-8').split("\n")
        event_format = {"fields": []}
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
                events_format[event_format["id"]] = event_format
                event_format = {"fields": []}
        return events_format

    def parse_cmd_lines(self, data: List) -> dict:
        cmd_lines = {}
        cmd_lines_list = data.decode('utf-8').split("\n")
        for cmd_line in cmd_lines_list:
            pos = cmd_line.find(" ")
            if pos == -1:
                continue
            cmd_lines[int(cmd_line[:pos])] = cmd_line[pos + 1:]
        return cmd_lines

    def parse_tid_groups(self, data: List) -> dict:
        tgids = {}
        tgids_lines_list = data.decode('utf-8').split("\n")
        for tgids_line in tgids_lines_list:
            pos = tgids_line.find(" ")
            if pos == -1:
                continue
            tgids[int(tgids_line[:pos])] = int(tgids_line[pos + 1:])
        return tgids

    def get_segment_data(self, segment_size) -> List:
        return self.trace_file.read_data(segment_size)

    def get_context(self) -> TraceParseContext:
        return self.context

    def get_viewer(self) -> TraceViewerInterface:
        return self.trace_viewer

    def parse(self) -> None:
        self.trace_format.accept(self)
        self.trace_viewer.calculate(self.context)
        pass


def parse_binary_trace_file() -> None:
    global binary_file
    global out_file
    file = TraceFile(binary_file)
    vformat = TraceFileFormat()
    viewer = TraceViewer([
        SysTraceViewer(out_file)
    ])
    context = TraceParseContext()
    parser = TraceFileParser(file, vformat, viewer, context)
    parser.parse()
    pass


def main() -> None:
    parse_options()

    if file_dir == '':
        if text_file != '':
            parse_text_trace_file()
        else:
            parse_binary_trace_file()
    else:
        for file in os.listdir(file_dir):
            if file.find('.sys') != -1:
                print(file)
                global binary_file
                global out_file
                binary_file = os.path.join(file_dir, file)
                out_file = os.path.join(os.path.split(binary_file)[0],
                                        os.path.split(binary_file)[-1].split('.')[0] + '.ftrace')
                parse_binary_trace_file()


if __name__ == '__main__':
    main()
