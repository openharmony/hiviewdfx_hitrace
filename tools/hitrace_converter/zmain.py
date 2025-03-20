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
    CONTEXT_CMD_LINES = 1
    def __init__(self) -> None:
        self.cpu_num = 1
        pass

    def set_param(self, key: int, value: Any) -> None:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            self.cpu_num = 1
            return
        if TraceParseContext.CONTEXT_CMD_LINES == key:
            self.cmd_lines = 1
            return
        pass

    def get_param(self, key: int) -> Any:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            return self.cpu_num
        if TraceParseContext.CONTEXT_CMD_LINES == key:
            return self.cmd_lines
        pass


class TraceFileParserInterface(metaclass = ABCMeta):
    """"
    功能描述: 声明解析HiTrace文件的接口
    """
    @abstractmethod
    def parse_field(self, field: Field) -> tuple:
        return ()

    @abstractmethod
    def get_context(self) -> TraceParseContext:
        return None

    @abstractmethod
    def get_segment_data(self, segment_size) -> List:
        return None

class TraceFileFormatInterface(metaclass = ABCMeta):
    """"
    功能描述: 声明HiTrace文件的接口
    """
    def __init__(self) -> None:
        pass


class TraceViewerInterface(metaclass = ABCMeta):
    """"
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


class FileHeader(FieldOperator):
    FIELD_TYPE = FieldType.TRACE_FILE_HEADER
    FILED_FORMAT = DataType.to_format([DataType.H, DataType.B, DataType.H, DataType.L])
    FIELD_SIZE = DataType.get_data_bytes(FILED_FORMAT)

    ITEM_MAGIC_NUMBER = 0
    ITEM_FILE_TYPE = 1
    ITEM_VERSION_NUMBER = 2
    ITEM_RESERVED = 3
    """"
    功能描述: 声明HiTrace文件的头部格式
    """
    def __init__(self) -> None:
        super().__init__(
            FileHeader.FIELD_TYPE,
            FileHeader.FIELD_SIZE,
            FileHeader.FILED_FORMAT,
            [
                FileHeader.ITEM_MAGIC_NUMBER,
                FileHeader.ITEM_FILE_TYPE,
                FileHeader.ITEM_VERSION_NUMBER,
                FileHeader.ITEM_RESERVED
            ]
        )

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        values = parser.parse_field(self)
        if len(values) == 0:
            return False
        cpu_num = (values[FileHeader.ITEM_RESERVED] >> 1) & 0b00011111
        context = parser.get_context()
        context.set_param(TraceParseContext.CONTEXT_CPU_NUM, cpu_num)
        return True


class RawTraceSegment(Field):
    """"
    功能描述: 声明HiTrace文件raw trace的段
    """

    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass


class EventFormatSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass


class CmdLinesSegment(FieldOperator):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """
    FIELD_TYPE = FieldType.SEGMENT_CMDLINES
    FILED_FORMAT = ""
    FIELD_SIZE = -1

    def __init__(self) -> None:
        super().__init__(
            CmdLinesSegment.FIELD_TYPE,
            CmdLinesSegment.FIELD_SIZE,
            CmdLinesSegment.FILED_FORMAT,
            [
            ]
        )
        pass

    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        cmd_lines = {}
        cmd_lines_list = segment.decode('utf-8').split("\n")
        for cmd_line in cmd_lines_list:
            pos = cmd_line.find(" ")
            if pos == -1:
                continue
            cmd_lines[int(cmd_line[:pos])] = cmd_line[pos + 1:]
        print(cmd_lines)
        return True


class TidGroupsSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass


class PrintkFormatSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass


class KallSymsSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass



class UnSupportSegment(FieldOperator):
    """"
    功能描述: 声明HiTrace文件还不支持解析的段
    """
    def __init__(self) -> None:
        # super().__init__(type, size, format, item_types)
        pass

    def accept(self, parser, segment: List = []) -> bool:
        print("unsupport")
        return True


class SegmentWrapper(FieldOperator):
    """"
    功能描述: 声明HiTrace文件包含所有段的格式
    """
    FIELD_TYPE = FieldType.SEGMENT_SEGMENTS
    FILED_FORMAT = DataType.to_format([DataType.I, DataType.I])
    FIELD_SIZE = DataType.get_data_bytes(FILED_FORMAT)

    ITEM_SEGMENT_TYPE = 0
    ITEM_SEGMENT_SIZE = 1
    def __init__(self, fields: List) -> None:
        super().__init__(
            SegmentWrapper.FIELD_TYPE,
            SegmentWrapper.FIELD_SIZE,
            SegmentWrapper.FILED_FORMAT,
            [
                SegmentWrapper.ITEM_SEGMENT_TYPE,
                SegmentWrapper.ITEM_SEGMENT_SIZE,
            ]
        )
        self.fields = fields
        pass


    def _getSegment(self, segment_type: int, cpu_num: int) -> FieldOperator:
        for field in self.fields:
            if field.FIELD_TYPE == segment_type:
                return field
            if (segment_type >= field.FIELD_TYPE) and (segment_type < field.FIELD_TYPE + cpu_num):
                return field
        return UnSupportSegment()


    def accept(self, parser: TraceFileParserInterface, segment: List = []) -> bool:
        while True:
            values = parser.parse_field(self)
            if len(values) == 0:
                break
            segment_type = values[SegmentWrapper.ITEM_SEGMENT_TYPE]
            segment_size = values[SegmentWrapper.ITEM_SEGMENT_SIZE]
            conext = parser.get_context()
            segment = self._getSegment(segment_type, conext.get_param(TraceParseContext.CONTEXT_CPU_NUM))
            segment_data = parser.get_segment_data(segment_size)
            segment.accept(parser, segment_data)
            pass


class TraceFile:
    """"
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
        """"
        功能描述: 从文件当前位置读取blockSize字节的内容
        参数: 读取字节数
        返回值: 文件内容
        """
        if (self.cur_post + block_size) > self.size:
            return None
        self.cur_post = self.cur_post + block_size
        return self.file.read(block_size)


class TraceFileFormat(TraceFileFormatInterface, OperatorInterface):
    def __init__(self) -> None:
        self.fields = [
            FileHeader(),
            SegmentWrapper([
                CmdLinesSegment(),
                # TidGroupsSegment(),
                # EventFormatSegment(),
                # PrintkFormatSegment(),
                # KallSymsSegment(),
                # RawTraceSegment()
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

    def parse_field(self, field: Field) -> tuple:
        data = self.trace_file.read_data(field.field_size)
        if data is None:
            return ()
        unpack_value = struct.unpack(field.format, data)
        return unpack_value

    def get_context(self) -> TraceParseContext:
        return self.context

    def get_segment_data(self, segment_size) -> List:
        return self.trace_file.read_data(segment_size)


def main() -> None:
    file = TraceFile(r"F:\source\gitee\MyOpenHarmony\hiviewdfx_hitrace\test\unittest\tools\sample\record_trace_20250320101116@2500-814308989.sys")
    format = TraceFileFormat()
    viewer = TraceViewer()
    context = TraceParseContext()
    parser = TraceFileParser(file, format, viewer, context)
    parser.parse()
    pass



if __name__ == '__main__':
    main()
