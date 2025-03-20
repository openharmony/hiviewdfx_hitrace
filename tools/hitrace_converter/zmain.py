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
    def getDataBytes(dataTypes: str) -> int:
        return struct.calcsize(dataTypes)

    @staticmethod
    def toFormat(dataTypes: List) -> str:
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
    def __init__(self, type: int, size: int, format: str, itemTypes: List) -> None:
        self.fieldType = type
        self.fieldSize = size
        self.format = format
        self.itemTypes = itemTypes
        pass


class TraceParseContext:
    CONTEXT_CPU_NUM = 1
    def __init__(self):
        self.cpuNum = 1
        pass

    def setParam(self, key: int, value: Any) -> None:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            self.cpuNum = 1
        pass

    def getParam(self, key: int) -> Any:
        if TraceParseContext.CONTEXT_CPU_NUM == key:
            return self.cpuNum
        pass


class TraceFileParserInterface(metaclass = ABCMeta):
    """"
    功能描述: 声明解析HiTrace文件的接口
    """
    @abstractmethod
    def parseField(self, field: Field) -> tuple:
        return ()

    @abstractmethod
    def getContext(self) -> TraceParseContext:
        return None

    @abstractmethod
    def getSegmentData(self, segmentSize) -> List:
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
    def accept(self, parser: TraceFileParserInterface, segment: int | List = 0) -> bool:
        return True


class FieldOperator(Field, OperatorInterface):
    def __init__(self, type, size, format, itemTypes):
        Field.__init__(self, type, size, format, itemTypes)
    pass

    def accept(self, parser: TraceFileParserInterface, segment: int | List = 0) -> bool:
        return True


class FileHeader(FieldOperator):
    FIELD_TYPE = FieldType.TRACE_FILE_HEADER
    FILED_FORMAT = DataType.toFormat([DataType.H, DataType.B, DataType.H, DataType.L])
    FIELD_SIZE = DataType.getDataBytes(FILED_FORMAT)

    ITEM_MAGIC_NUMBER = 0
    ITEM_FILE_TYPE = 1
    ITEM_VERSION_NUMBER = 2
    ITEM_RESERVED = 3
    """"
    功能描述: 声明HiTrace文件的头部格式
    """
    def __init__(self):
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

    def accept(self, parser: TraceFileParserInterface, segment: int | List = 0) -> bool:
        values = parser.parseField(self)
        if len(values) == 0:
            return False
        cpuNum = (values[FileHeader.ITEM_RESERVED] >> 1) & 0b00011111
        context = parser.getContext()
        context.setParam(TraceParseContext.CONTEXT_CPU_NUM, cpuNum)
        return True


class RawTraceSegment(Field):
    """"
    功能描述: 声明HiTrace文件raw trace的段
    """

    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass


class EventFormatSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass


class CmdLinesSegment(FieldOperator):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """
    FIELD_TYPE = FieldType.SEGMENT_CMDLINES
    FILED_FORMAT = ""
    FIELD_SIZE = -1

    def __init__(self):
        super().__init__(
            CmdLinesSegment.FIELD_TYPE,
            CmdLinesSegment.FIELD_SIZE,
            CmdLinesSegment.FILED_FORMAT,
            [
            ]
        )
        pass

    def accept(self, parser: TraceFileParserInterface, segment: int | List = 0) -> bool:
        print("CmdLinesSegment")
        return True


class TidGroupsSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass


class PrintkFormatSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass


class KallSymsSegment(Field):
    """"
    功能描述: 声明HiTrace文件event格式的段
    """

    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass



class UnSupportSegment(FieldOperator):
    """"
    功能描述: 声明HiTrace文件还不支持解析的段
    """
    def __init__(self):
        # super().__init__(type, size, format, itemTypes)
        pass
    def accept(self, parser, segment = 0) -> bool:
        print("unsupport")
        return True


class SegmentWrapper(FieldOperator):
    """"
    功能描述: 声明HiTrace文件包含所有段的格式
    """
    FIELD_TYPE = FieldType.SEGMENT_SEGMENTS
    FILED_FORMAT = DataType.toFormat([DataType.I, DataType.I])
    FIELD_SIZE = DataType.getDataBytes(FILED_FORMAT)

    ITEM_SEGMENT_TYPE = 0
    ITEM_SEGMENT_SIZE = 1
    def __init__(self, fields: List):
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


    def _getSegment(self, segmentType: int, cpuNum: int) -> FieldOperator:
        for field in self.fields:
            if field.FIELD_TYPE == segmentType:
                return field
            if (segmentType >= field.FIELD_TYPE) and (segmentType < field.FIELD_TYPE + cpuNum):
                return field
        return UnSupportSegment()


    def accept(self, parser: TraceFileParserInterface, segment: int | List = 0) -> bool:
        while True:
            values = parser.parseField(self)
            if len(values) == 0:
                break
            segmentType = values[SegmentWrapper.ITEM_SEGMENT_TYPE]
            segmentSize = values[SegmentWrapper.ITEM_SEGMENT_SIZE]
            conext = parser.getContext()
            segment = self._getSegment(segmentType, conext.getParam(TraceParseContext.CONTEXT_CPU_NUM))
            segmentData = parser.getSegmentData(segmentSize)
            segment.accept(parser, segmentData)
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
        self.curPost = 0
        pass

    def readData(self, blockSize: int) -> List:
        """"
        功能描述: 从文件当前位置读取blockSize字节的内容
        参数: 读取字节数
        返回值: 文件内容
        """
        if (self.curPost + blockSize) > self.size:
            return None
        self.curPost = self.curPost + blockSize
        return self.file.read(blockSize)


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


    def accept(self, parser: TraceFileParserInterface, segment = 0) -> bool:
        for field in self.fields:
            if field.accept(parser) is False:
                return False
        return True


class TraceViewer(TraceViewerInterface):
    def __init__(self) -> None:
        pass


class TraceFileParser(TraceFileParserInterface):
    def __init__(self, file: TraceFile, format: TraceFileFormat, viewer: TraceViewer, context: TraceParseContext) -> None:
        self.traceFile = file
        self.traceFormat = format
        self.traceViewer = viewer
        self.context = context
        pass

    def parse(self) -> None:
        self.traceFormat.accept(self)
        pass

    def parseField(self, field: Field) -> tuple:
        data = self.traceFile.readData(field.fieldSize)
        if data is None:
            return ()
        unpackValue = struct.unpack(field.format, data)
        return unpackValue

    def getContext(self):
        return self.context

    def getSegmentData(self, segmentSize) -> List:
        return self.traceFile.readData(segmentSize)


def main() -> None:
    file = TraceFile(r"C:\source\gitee\OpenHarmony\hiviewdfx_hitrace\test\unittest\tools\sample\record_trace_20250320101116@2500-814308989.sys")
    format = TraceFileFormat()
    viewer = TraceViewer()
    context = TraceParseContext()
    parser = TraceFileParser(file, format, viewer, context)
    parser.parse()
    pass



if __name__ == '__main__':
    main()
