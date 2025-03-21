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

from enum import IntEnum, unique
from abc import ABCMeta, abstractmethod
from typing import List, Any
import os
import struct
import stat
import copy

class DataType:
    # 数据类型
    B = "B"
    H = "H"
    I = "I"
    L = "L"
    Q = "Q"

    # 数据类型字节数
    DATA_BYTE_INT8 = 1
    DATA_BYTE_INT16 = 2
    DATA_BYTE_INT32 = 4
    DATA_BYTE_INT64 = 8

    dataTypes = {
        B : DATA_BYTE_INT8,
        H : DATA_BYTE_INT16,
        I : DATA_BYTE_INT32,
        L : DATA_BYTE_INT32,
        Q : DATA_BYTE_INT64,
    }

    @staticmethod
    def get_data_bytes(dataTypes: str) -> int:
        return struct.calcsize(dataTypes)

    @staticmethod
    def to_format(dataTypes: List) -> str:
        return "".join(dataTypes)


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
    def __init__(self, type: int, size: int, format: str, item_types: List) -> None:
        self.field_type = type
        self.field_size = size
        self.format = format
        self.item_types = item_types
        pass


class TraceParseContext:
    CONTEXT_CPU_NUM = 1
    CONTEXT_CMD_LINES = 2
    CONTEXT_EVENT_SIZE = 3

    def __init__(self) -> None:
        self.cpu_num = 1
        self.cmd_lines = []
        self.event_size = 0
        pass

    def set_param(self, key: int, value: Any) -> None:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            self.cpu_num = value
            return
        if TraceParseContext.CONTEXT_CMD_LINES == key:
            self.cmd_lines = value
            return
        if TraceParseContext.CONTEXT_EVENT_SIZE == key:
            self.event_size = value
            return
        pass

    def get_param(self, key: int) -> Any:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            return self.cpu_num
        if TraceParseContext.CONTEXT_CMD_LINES == key:
            return self.cmd_lines
        if TraceParseContext.CONTEXT_EVENT_SIZE == key:
            return self.event_size
        pass


class TraceFileParserInterface(metaclass = ABCMeta):
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
    def parse_tid_gropus(self, data: List) -> dict:
        return {}

    @abstractmethod
    def get_context(self) -> TraceParseContext:
        return None

    @abstractmethod
    def get_segment_data(self, segment_size) -> List:
        return None


class TraceViewerInterface(metaclass = ABCMeta):
    """
    功能描述: 声明解析HiTrace结果的接口
    """
    def __init__(self) -> None:
        pass


class OperatorInterface(metaclass = ABCMeta):
    @abstractmethod
    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        return True


class FieldOperator(Field, OperatorInterface):
    def __init__(self, type, size, format, item_types) -> None:
        Field.__init__(self, type, size, format, item_types)
    pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        return True


class SegmentOperator(Field, OperatorInterface):
    def __init__(self, type) -> None:
        Field.__init__(self, type, -1, "", [])
    pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        return True


class FileHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的头部格式
    """
    # 描述HiTrace文件头的pack格式
    FORMAT = "HBHL"

    ITEM_MAGIC_NUMBER = 0
    ITEM_FILE_TYPE = 1
    ITEM_VERSION_NUMBER = 2
    ITEM_RESERVED = 3
    def __init__(self, item_types: List = []) -> None:
        super().__init__(
            FieldType.TRACE_FILE_HEADER,
            DataType.get_data_bytes(FileHeader.FORMAT),
            FileHeader.FORMAT,
            item_types
        )

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        data = parser.get_segment_data(self.field_size)
        values = parser.parse_simple_field(self.format, data)
        if len(values) == 0:
            return False
        cpu_num = (values[FileHeader.ITEM_RESERVED] >> 1) & 0b00011111
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_CPU_NUM, cpu_num)
        print("cup number %d" % (cpu_num))
        return True


class PageHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的第个 raw event 页头部格式
    """
    # 描述HiTrace文件头的pack格式
    FORMAT = "QQB"

    ITEM_TIMESTAMP = 0
    ITEM_LENGTH = 1
    ITEM_CORE_ID = 2
    def __init__(self) -> None:
        super().__init__(
            FieldType.TRACE_EVENT_PAGE_HEADER,
            DataType.get_data_bytes(PageHeader.FORMAT),
            PageHeader.FORMAT,
            [
                PageHeader.ITEM_TIMESTAMP,
                PageHeader.ITEM_LENGTH,
                PageHeader.ITEM_CORE_ID,
            ]
        )

    def accept(self, parser: TraceFileParserInterface, segment = []) -> bool:
        value = parser.parse_simple_field(self.format, segment)
        return True
    pass


class TraceEventHeader(FieldOperator):
    """
    功能描述: 声明HiTrace文件的第个 trace event 头部格式
    """
    # 描述HiTrace文件trace event的pack格式
    FORMAT = "LH"
    RMQ_ENTRY_ALIGN_MASK = 3

    ITEM_TIMESTAMP_OFFSET = 0
    ITEM_EVENT_SIZE = 1

    def __init__(self) -> None:
        super().__init__(
            FieldType.TRACE_EVENT_HEADER,
            DataType.get_data_bytes(TraceEventHeader.FORMAT),
            TraceEventHeader.FORMAT,
            [
                TraceEventHeader.ITEM_TIMESTAMP_OFFSET,
                TraceEventHeader.ITEM_EVENT_SIZE
            ]
        )

    def accept(self, parser: TraceFileParserInterface, segment = []) -> bool:
        value  = parser.parse_simple_field(self.format, segment)
        if len(value) == 0:
            return False
        event_size = value[TraceEventHeader.ITEM_EVENT_SIZE]
        event_size = ((event_size + TraceEventHeader.RMQ_ENTRY_ALIGN_MASK) & (~TraceEventHeader.RMQ_ENTRY_ALIGN_MASK))
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_EVENT_SIZE, event_size)
        return True
    pass


class TraceEventContent(OperatorInterface):
    def accept(self, parser: TraceFileParserInterface, segment = []) -> bool:
        return True
    pass


class TraceEventWrapper(OperatorInterface):
    def __init__(self, event_header: TraceEventHeader, event_content: TraceEventContent) -> None:
        self.event_header = event_header
        self.event_content = event_content
        pass

    def _nextEventHeader(self, cur_post: int, segment: List) -> tuple:
        data = segment[cur_post: cur_post + self.event_header.field_size]
        next_post = cur_post + self.event_header.field_size
        return (next_post, data)
        pass

    def _nextEventContent(self, cur_post: int, segment: List, parser: TraceFileParserInterface) -> tuple:
        context = parser.get_context()
        event_size = context.get_param(TraceParseContext.CONTEXT_EVENT_SIZE)
        data = segment[cur_post: cur_post + event_size]
        next_post = cur_post + event_size
        return (next_post, data)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        cur_post = 0
        while cur_post < len(segment):
            (cur_post, data) = self._nextEventHeader(cur_post, segment)
            if self.event_header.accept(parser, data) is False:
                break
            (cur_post, data) = self._nextEventContent(cur_post, segment, parser)
            if self.event_content.accept(parser, data) is False:
                break
        return True
    pass


class PageWrapper(OperatorInterface):
    TRACE_PAGE_SIZE = 4096

    def __init__(self, page_header: PageHeader, trace_event: TraceEventWrapper) -> None:
        self.page_header = page_header
        self.trace_event = trace_event
        pass

    def _next(self, cur_post: int, segment: List) -> tuple:
        if (cur_post + PageWrapper.TRACE_PAGE_SIZE) > len(segment):
            return (-1, None)
        data = segment[cur_post: cur_post + PageWrapper.TRACE_PAGE_SIZE]
        nextPost = cur_post + PageWrapper.TRACE_PAGE_SIZE
        return (nextPost, data)

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        cur_post = 0
        while True:
            (cur_post, data) = self._next(cur_post, segment)
            if data is None:
                break

            self.page_header.accept(parser, data[0: self.page_header.field_size])
            self.trace_event.accept(parser, data[self.page_header.field_size: ])
        return True
    pass


class RawTraceSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 trace_pipe_raw 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_RAW_TRACE)
        self.field = PageWrapper(
            PageHeader(),
            TraceEventWrapper(
                TraceEventHeader(),
                TraceEventContent(),
            )
        )
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("raw event segment")
        self.field.accept(parser, segment)
        return True



class EventFormatSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 event/format 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_EVENTS_FORMAT)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("event format segment")
        parser.parse_event_format(segment)
        return True


class CmdLinesSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 saved_cmdlines 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_CMDLINES)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("cmd lines segment")
        parser.parse_cmd_lines(segment)
        return True


class TidGroupsSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 saved_tgids 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_TGIDS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("tid groups segment")
        parser.parse_tid_gropus(segment)
        return True


class PrintkFormatSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 printk_formats 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_PRINTK_FORMATS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("printk format segment")
        return True


class KallSymsSegment(SegmentOperator):
    """
    功能描述: 声明HiTrace文件 Kall Sysms 内容的段格式
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_KALLSYMS)
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        print("kall sysms segment")
        return True


class UnSupportSegment(FieldOperator):
    """
    功能描述: 声明HiTrace文件还不支持解析的段
    """
    def __init__(self) -> None:
        super().__init__(FieldType.SEGMENT_UNSUPPORT, -1, "", [])
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
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


    def _getSegment(self, segment_type: int, cpu_num: int) -> FieldOperator:
        for field in self.fields:
            if field.field_type == segment_type:
                return field

        for field in self.fields:
            if (segment_type >= field.field_type) and (segment_type < field.field_type + cpu_num):
                return field
        return UnSupportSegment()


    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        while True:
            data = parser.get_segment_data(self.field_size)
            values = parser.parse_simple_field(self.format, data)
            if len(values) == 0:
                break
            segment_type = values[SegmentWrapper.ITEM_SEGMENT_TYPE]
            segment_size = values[SegmentWrapper.ITEM_SEGMENT_SIZE]
            conext = parser.get_context()

            print("segment type=%d, size=%d"%(segment_type, segment_size))
            segment = self._getSegment(segment_type, conext.get_param(TraceParseContext.CONTEXT_CPU_NUM))
            segment_data = parser.get_segment_data(segment_size)
            segment.accept(parser, segment_data)
            pass
        return True


class TraceFile:
    """
    功能描述: 读取hitrace的二进制文件
    """
    def __init__(self, name: str) -> None:
        self.name = name
        flags  = os.O_RDONLY | os.O_BINARY
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
                PrintkFormatSegment(),
                KallSymsSegment(),
                RawTraceSegment()
            ])
        ]
        pass


    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        for field in self.fields:
            if field.accept(parser) is False:
                return False
        return True


class TraceViewer(TraceViewerInterface):
    def __init__(self) -> None:
        pass


class TraceFileParser(TraceFileParserInterface):
    def __init__(self, file: TraceFile, format: TraceFileFormat, viewer: TraceViewer, context: TraceParseContext) -> None:
        self.trace_file = file
        self.trace_format = format
        self.trace_viewer = viewer
        self.context = context
        pass

    def parse(self) -> None:
        self.trace_format.accept(self)
        pass

    def parse_simple_field(self, format: str, data: List) -> tuple:
        if data is None:
            return ()
        if DataType.get_data_bytes(format) != len(data):
            return ()
        unpack_value = struct.unpack(format, data)
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
        # print(events_format)
        return events_format


    def parse_cmd_lines(self, data: List) -> dict:
        cmd_lines = {}
        cmd_lines_list = data.decode('utf-8').split("\n")
        for cmd_line in cmd_lines_list:
            pos = cmd_line.find(" ")
            if pos == -1:
                continue
            cmd_lines[int(cmd_line[:pos])] = cmd_line[pos + 1:]
        # print(cmd_lines)
        return cmd_lines

    def parse_tid_gropus(self, data: List) -> dict:
        tgids = {}
        tgids_lines_list = data.decode('utf-8').split("\n")
        for tgids_line in tgids_lines_list:
            pos = tgids_line.find(" ")
            if pos == -1:
                continue
            tgids[int(tgids_line[:pos])] = int(tgids_line[pos + 1:])
        return tgids

    def get_context(self) -> TraceParseContext:
        return self.context

    def get_segment_data(self, segment_size) -> List:
        return self.trace_file.read_data(segment_size)


def main() -> None:
    file = TraceFile(r"test\unittest\tools\sample\record_trace_20250320101116@2500-814308989.sys")
    format = TraceFileFormat()
    viewer = TraceViewer()
    context = TraceParseContext()
    parser = TraceFileParser(file, format, viewer, context)
    parser.parse()
    pass



if __name__ == '__main__':
    main()
