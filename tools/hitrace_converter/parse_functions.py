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

def parse_bytes_to_str(data):
    return data[:data.index(b'\x00')].decode('utf-8')


def parse_int_field(one_event, name, int_signed):
    return int.from_bytes(one_event["fields"][name], byteorder='little', signed=int_signed)


def parse(event_print_fmt, data, one_event):
    result = None
    parse_func = print_fmt_func_map.get(event_print_fmt, "")
    if parse_func != "":
        result = parse_func(data, one_event)
    return result


def parse_sched_wakeup_hm(data, one_event):
    pname = parse_bytes_to_str(one_event["fields"]["pname[16]"])
    pid = parse_int_field(one_event, "pid", True)
    prio = parse_int_field(one_event, "prio", True)
    success = parse_int_field(one_event, "success", True)
    target_cpu = parse_int_field(one_event, "target_cpu", True)

    return "comm=%s pid=%d prio=%d target_cpu=%03d" % (pname, pid, prio, target_cpu)


def parse_sched_wakeup(data, one_event):
    comm = parse_bytes_to_str(one_event["fields"]["comm[16]"])
    pid = parse_int_field(one_event, "pid", True)
    prio = parse_int_field(one_event, "prio", True)
    success = parse_int_field(one_event, "success", True)
    target_cpu = parse_int_field(one_event, "target_cpu", True)

    return "comm=%s pid=%d prio=%d target_cpu=%03d" % (comm, pid, prio, target_cpu)


def parse_sched_switch_hm(data, one_event):
    pname = parse_bytes_to_str(one_event["fields"]["pname[16]"])
    prev_tid = parse_int_field(one_event, "prev_tid", True)
    pprio = parse_int_field(one_event, "pprio", True)
    pstate = parse_int_field(one_event, "pstate", True)
    nname = parse_bytes_to_str(one_event["fields"]["nname[16]"])
    next_tid = parse_int_field(one_event, "next_tid", True)
    nprio = parse_int_field(one_event, "nprio", True)

    pstate_map = {0x0: 'R', 0x1: 'S', 0x2: 'D', 0x10: 'X', 0x100: 'R+'}
    prev_state = pstate_map.get(pstate, '?')

    return "prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s ==> next_comm=%s next_pid=%d next_prio=%d" \
    % (pname, prev_tid, pprio, prev_state, nname, next_tid, nprio)


def parse_sched_switch(data, one_event):
    prev_comm = parse_bytes_to_str(one_event["fields"]["prev_comm[16]"])
    prev_pid = parse_int_field(one_event, "prev_pid", True)
    prev_prio = parse_int_field(one_event, "prev_prio", True)
    prev_state = parse_int_field(one_event, "prev_state", True)
    next_comm = parse_bytes_to_str(one_event["fields"]["next_comm[16]"])
    next_pid = parse_int_field(one_event, "next_pid", True)
    next_prio = parse_int_field(one_event, "next_prio", True)
    expeller_type = parse_int_field(one_event, "expeller_type", False)

    pstate_map = {0x1: 'S', 0x2: 'D', 0x4: 'T', 0x8: 't', 0x10: 'X', 0x20: 'Z', 0x40: 'P', 0x80: 'I'}
    pstate = pstate_map.get(prev_state & 0xff, 'R')
    if prev_state & 0x100 != 0:
        pstate += '+'

    return "prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s ==> next_comm=%s next_pid=%d next_prio=%d expeller_type=%d" \
    % (prev_comm, prev_pid, prev_prio, pstate, next_comm, next_pid, next_prio, expeller_type)


def parse_sched_blocked_reason_hm(data, one_event):
    pid = parse_int_field(one_event, "pid", True)
    caller = parse_int_field(one_event, "caller", False)
    iowait = parse_int_field(one_event, "iowait", False)
    delay = parse_int_field(one_event, "delay", False)
    cnode_idx = parse_int_field(one_event, "cnode_idx", False)

    return "pid=%d iowait=%d caller=0x%x cnode_idx=%d delay=%d" % (pid, iowait, caller, cnode_idx, delay >> 10)


def parse_sched_blocked_reason(data, one_event):
    pid = parse_int_field(one_event, "pid", True)
    caller = parse_int_field(one_event, "caller", False)
    iowait = parse_int_field(one_event, "iowait", False)
    delay = parse_int_field(one_event, "delay", False)

    return "pid=%d iowait=%d caller=0x%x delay=%d" % (pid, iowait, caller, delay >> 10)


def parse_cpu_frequency(data, one_event):
    state = parse_int_field(one_event, "state", False)
    cpu_id = parse_int_field(one_event, "cpu_id", False)

    return "state=%d cpu_id=%d" % (state, cpu_id)


def parse_clock_set_rate(data, one_event):
    data_pos = parse_int_field(one_event, "name", False) & 0xffff
    state = parse_int_field(one_event, "state", False)
    cpu_id = parse_int_field(one_event, "cpu_id", False)

    return "%s state=%d cpu_id=%d" % (parse_bytes_to_str(data[data_pos:]), state, cpu_id)


def parse_cpu_frequency_limits_hm(data, one_event):
    min_freq = parse_int_field(one_event, "min", False)
    max_freq = parse_int_field(one_event, "max", False)
    cpu_id = parse_int_field(one_event, "cpu_id", False)

    return "min=%d max=%d cpu_id=%d" % (min_freq, max_freq, cpu_id)


def parse_cpu_frequency_limits(data, one_event):
    min_freq = parse_int_field(one_event, "min_freq", False)
    max_freq = parse_int_field(one_event, "max_freq", False)
    cpu_id = parse_int_field(one_event, "cpu_id", False)

    return "min=%d max=%d cpu_id=%d" % (min_freq, max_freq, cpu_id)


def parse_cpu_idle(data, one_event):
    state = parse_int_field(one_event, "state", False)
    cpu_id = parse_int_field(one_event, "cpu_id", False)

    return "state=%d cpu_id=%d" % (state, cpu_id)


def parse_ext4_da_write_begin(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    ino = parse_int_field(one_event, "ino", False)
    pos = parse_int_field(one_event, "pos", True)
    len_write = parse_int_field(one_event, "len", False)
    flags = parse_int_field(one_event, "flags", False)

    return "dev %d,%d ino %d pos %d len %d flags %d" \
    % (dev >> 20, dev & 0xfffff, ino, pos, len_write, flags)


def parse_ext4_da_write_end(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    ino = parse_int_field(one_event, "ino", False)
    pos = parse_int_field(one_event, "pos", True)
    len_write = parse_int_field(one_event, "len", False)
    copied = parse_int_field(one_event, "copied", False)

    return "dev %d,%d ino %d pos %d len %d copied %d" \
    % (dev >> 20, dev & 0xfffff, ino, pos, len_write, copied)


def parse_ext4_sync_file_enter(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    ino = parse_int_field(one_event, "ino", False)
    parent = parse_int_field(one_event, "parent", False)
    datasync = parse_int_field(one_event, "datasync", True)

    return "dev %d,%d ino %d parent %d datasync %d " \
    % (dev >> 20, dev & 0xfffff, ino, parent, datasync)


def parse_ext4_sync_file_exit(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    ino = parse_int_field(one_event, "ino", False)
    ret = parse_int_field(one_event, "ret", True)
    return "dev %d,%d ino %d ret %d" % (dev >> 20, dev & 0xfffff, ino, ret)


def parse_block_bio_remap(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    sector = parse_int_field(one_event, "sector", False)
    nr_sector = parse_int_field(one_event, "nr_sector", False)
    old_dev = parse_int_field(one_event, "old_dev", False)
    old_sector = parse_int_field(one_event, "old_sector", False)
    rwbs = parse_bytes_to_str(one_event["fields"]["rwbs[8]"])

    return "%d,%d %s %d + %d <- (%d,%d) %d" \
    % (dev >> 20, dev & 0xfffff, rwbs, sector, nr_sector, old_dev >> 20, old_dev & 0xfffff, old_sector)


def parse_block_rq_issue_hm(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    sector = parse_int_field(one_event, "sector", False)
    nr_sector = parse_int_field(one_event, "nr_sector", False)
    bytes_num = parse_int_field(one_event, "bytes", False)
    rwbs = parse_bytes_to_str(one_event["fields"]["rwbs[8]"])
    comm = parse_bytes_to_str(one_event["fields"]["comm[16]"])
    cmd = parse_bytes_to_str(one_event["fields"]["cmd[16]"])

    return "%d,%d %s %d (%s) %d + %d [%s]" \
    % (dev >> 20, dev & 0xfffff, rwbs, bytes_num, cmd, sector, nr_sector, comm)


def parse_block_rq_issue_or_insert(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    sector = parse_int_field(one_event, "sector", False)
    nr_sector = parse_int_field(one_event, "nr_sector", False)
    bytes_num = parse_int_field(one_event, "bytes", False)
    rwbs = parse_bytes_to_str(one_event["fields"]["rwbs[8]"])
    comm = parse_bytes_to_str(one_event["fields"]["comm[16]"])
    cmd_pos = parse_int_field(one_event, "cmd", False) & 0xffff

    return "%d,%d %s %d (%s) %d + %d [%s]" \
    % (dev >> 20, dev & 0xfffff, rwbs, bytes_num, parse_bytes_to_str(data[cmd_pos:]), sector, nr_sector, comm)


def parse_block_rq_complete_hm(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    sector = parse_int_field(one_event, "sector", False)
    nr_sector = parse_int_field(one_event, "nr_sector", False)
    error = parse_int_field(one_event, "error", True)
    rwbs = parse_bytes_to_str(one_event["fields"]["rwbs[8]"])
    cmd = parse_bytes_to_str(one_event["fields"]["cmd[16]"])

    return "%d,%d %s (%s) %d + %d [%d]" \
    % (dev >> 20, dev & 0xfffff, rwbs, cmd, sector, nr_sector, error)


def parse_block_rq_complete(data, one_event):
    dev = parse_int_field(one_event, "dev", False)
    sector = parse_int_field(one_event, "sector", False)
    nr_sector = parse_int_field(one_event, "nr_sector", False)
    error = parse_int_field(one_event, "error", True)
    rwbs = parse_bytes_to_str(one_event["fields"]["rwbs[8]"])
    cmd_pos = parse_int_field(one_event, "cmd", False) & 0xffff

    return "%d,%d %s (%s) %d + %d [%d]" \
    % (dev >> 20, dev & 0xfffff, rwbs, parse_bytes_to_str(data[cmd_pos:]), sector, nr_sector, error)


def parse_ufshcd_command_hm(data, one_event):
    dev_name = parse_bytes_to_str(one_event["fields"]["dev_name[16]"])
    command_str = parse_bytes_to_str(one_event["fields"]["str[16]"])
    tag = parse_int_field(one_event, "tag", False)
    doorbell = parse_int_field(one_event, "doorbell", False)
    transfer_len = parse_int_field(one_event, "transfer_len", True)
    intr = parse_int_field(one_event, "intr", False)
    lba = parse_int_field(one_event, "lba", False)
    opcode = parse_int_field(one_event, "opcode", False)

    return "%s: %s: tag: %d, DB: 0x%x, size: %d, IS: %d, LBA: %d, opcode: 0x%x" \
    % (command_str, dev_name, tag, doorbell, transfer_len, intr, lba, opcode)


def parse_ufshcd_command(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    str_pos = parse_int_field(one_event, "str", False) & 0xffff
    tag = parse_int_field(one_event, "tag", False)
    doorbell = parse_int_field(one_event, "doorbell", False)
    transfer_len = parse_int_field(one_event, "transfer_len", True)
    intr = parse_int_field(one_event, "intr", False)
    lba = parse_int_field(one_event, "lba", False)
    opcode = parse_int_field(one_event, "opcode", False)
    group_id = parse_int_field(one_event, "group_id", False)

    opcode_map = {0x8a: "WRITE_16", 0x2a: "WRITE_10", 0x88: "READ_16", \
    0x28: "READ_10", 0x35: "SYNC", 0x42: "UNMAP"}

    return "%s: %s: tag: %d, DB: 0x%x, size: %d, IS: %d, LBA: %d, opcode: 0x%x (%s), group_id: 0x%x" \
    % (parse_bytes_to_str(data[str_pos:]), parse_bytes_to_str(data[dev_name_pos:]), tag, doorbell, \
    transfer_len, intr, lba, opcode, opcode_map.get(opcode, ""), group_id)


def parse_ufshcd_upiu(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    str_pos = parse_int_field(one_event, "str", False) & 0xffff
    hdr = parse_int_field(one_event, "hdr[12]", False)
    tsf = parse_int_field(one_event, "tsf[16]", False)

    return "%s: %s: HDR:0x%x, CDB:0x%x" % (parse_bytes_to_str(data[str_pos:]), \
    parse_bytes_to_str(data[dev_name_pos:]), hdr, tsf)


def parse_ufshcd_uic_command(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    str_pos = parse_int_field(one_event, "str", False) & 0xffff
    cmd = parse_int_field(one_event, "cmd", False)
    arg1 = parse_int_field(one_event, "arg1", False)
    arg2 = parse_int_field(one_event, "arg2", False)
    arg3 = parse_int_field(one_event, "arg3", False)

    return "%s: %s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x" \
    % (parse_bytes_to_str(data[str_pos:]), parse_bytes_to_str(data[dev_name_pos:]), cmd, arg1, arg2, arg3)


def parse_ufshcd_funcs(data, one_event):
    usecs = parse_int_field(one_event, "usecs", True)
    err = parse_int_field(one_event, "err", True)
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    dev_state = parse_int_field(one_event, "dev_state", True)
    link_state = parse_int_field(one_event, "link_state", True)

    dev_state_map = {1: "UFS_ACTIVE_PWR_MODE", 2: "UFS_SLEEP_PWR_MODE", 3: "UFS_POWERDOWN_PWR_MODE"}
    link_state_map = {0: "UIC_LINK_OFF_STATE", 1: "UIC_LINK_ACTIVE_STATE", 2: "UIC_LINK_HIBERN8_STATE"}

    return "%s: took %d usecs, dev_state: %s, link_state: %s, err %d" \
    % (parse_bytes_to_str(data[dev_name_pos:]), usecs, dev_state_map.get(dev_state, ""), \
    link_state_map.get(link_state, ""), err)


def parse_ufshcd_profile_funcs(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    profile_info_pos = parse_int_field(one_event, "profile_info", False) & 0xffff
    time_us = parse_int_field(one_event, "time_us", True)
    err = parse_int_field(one_event, "err", True)

    return "%s: %s: took %d usecs, err %d" \
    % (parse_bytes_to_str(data[dev_name_pos:]), parse_bytes_to_str(data[profile_info_pos:]), time_us, err)


def parse_ufshcd_auto_bkops_state(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    state_pos = parse_int_field(one_event, "state", False) & 0xffff

    return "%s: auto bkops - %s" % (parse_bytes_to_str(data[dev_name_pos:]), parse_bytes_to_str(data[state_pos:]))


def parse_ufshcd_clk_scaling(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    state_pos = parse_int_field(one_event, "state", False) & 0xffff
    clk_pos = parse_int_field(one_event, "clk", False) & 0xffff
    prev_state = parse_int_field(one_event, "prev_state", False)
    curr_state = parse_int_field(one_event, "curr_state", False)

    return "%s: %s %s from %d to %d Hz" % (parse_bytes_to_str(data[dev_name_pos:]), \
    parse_bytes_to_str(data[state_pos:]), parse_bytes_to_str(data[clk_pos:]), prev_state, curr_state)


def parse_ufshcd_clk_gating(data, one_event):
    dev_name_pos = parse_int_field(one_event, "dev_name", False) & 0xffff
    state = parse_int_field(one_event, "state", True)

    state_map = {0: "CLKS_OFF", 1: "CLKS_ON", 2: "REQ_CLKS_OFF", 3: "REQ_CLKS_ON"}

    return "%s: gating state changed to %s" % (parse_bytes_to_str(data[dev_name_pos:]), state_map.get(state, ''))


def parse_i2c_read(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    msg_nr = parse_int_field(one_event, "msg_nr", False)
    addr = parse_int_field(one_event, "addr", False)
    flags = parse_int_field(one_event, "flags", False)
    len_read = parse_int_field(one_event, "len", False)

    return "i2c-%d #%d a=%03x f=%04x l=%d" % (adapter_nr, msg_nr, addr, flags, len_read)


def parse_i2c_write_or_reply(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    msg_nr = parse_int_field(one_event, "msg_nr", False)
    addr = parse_int_field(one_event, "addr", False)
    flags = parse_int_field(one_event, "flags", False)
    len_write = parse_int_field(one_event, "len", False)
    buf_pos = parse_int_field(one_event, "buf", False) & 0xffff
    return ("i2c-%d #%d a=%03x f=%04x l=%d " % (adapter_nr, msg_nr, addr, flags, len_write)) \
    + "{:{width}d}".format(int(parse_bytes_to_str(data[buf_pos:])), width=len_write)


def parse_i2c_result(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    nr_msgs = parse_int_field(one_event, "nr_msgs", False)
    ret = parse_int_field(one_event, "ret", True)

    return "i2c-%d n=%d ret=%d" % (adapter_nr, nr_msgs, ret)


def parse_smbus_read(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    flags = parse_int_field(one_event, "flags", False)
    addr = parse_int_field(one_event, "addr", False)
    command = parse_int_field(one_event, "command", False)
    protocol = parse_int_field(one_event, "protocol", False)

    protocol_map = {0: "QUICK", 1: "BYTE", 2: "BYTE_DATA", 3: "WORD_DATA", \
    4: "PROC_CALL", 5: "BLOCK_DATA", 6: "I2C_BLOCK_BROKEN", 7: "BLOCK_PROC_CALL", 8: "I2C_BLOCK_DATA"}

    return "i2c-%d a=%03x f=%04x c=%x %s" % (adapter_nr, addr, flags, command, protocol_map.get(protocol, ''))


def parse_smbus_write_or_reply(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    addr = parse_int_field(one_event, "addr", False)
    flags = parse_int_field(one_event, "flags", False)
    command = parse_int_field(one_event, "command", False)
    len_write = parse_int_field(one_event, "len", False)
    protocol = parse_int_field(one_event, "protocol", False)
    buf = parse_bytes_to_str(one_event["fields"]["buf[32 + 2]"])

    protocol_map = {0: "QUICK", 1: "BYTE", 2: "BYTE_DATA", 3: "WORD_DATA", \
    4: "PROC_CALL", 5: "BLOCK_DATA", 6: "I2C_BLOCK_BROKEN", 7: "BLOCK_PROC_CALL", 8: "I2C_BLOCK_DATA"}

    return ("i2c-%d a=%03x f=%04x c=%x %s l=%d" \
    % (adapter_nr, addr, flags, command, protocol_map.get(protocol, ''), \
    len_write)) + "{:{width}d}".format(int(buf), width=len_write)


def parse_smbus_result(data, one_event):
    adapter_nr = parse_int_field(one_event, "adapter_nr", True)
    addr = parse_int_field(one_event, "addr", False)
    flags = parse_int_field(one_event, "flags", False)
    read_write_value = parse_int_field(one_event, "read_write", False)
    command = parse_int_field(one_event, "command", False)
    res = parse_int_field(one_event, "res", True)
    protocol = parse_int_field(one_event, "protocol", False)

    protocol_map = {0: "QUICK", 1: "BYTE", 2: "BYTE_DATA", 3: "WORD_DATA", \
    4: "PROC_CALL", 5: "BLOCK_DATA", 6: "I2C_BLOCK_BROKEN", 7: "BLOCK_PROC_CALL", 8: "I2C_BLOCK_DATA"}
    read_write = "wr" if read_write_value == 0 else "rd"

    return "i2c-%d a=%03x f=%04x c=%x %s %s res=%d" \
    % (adapter_nr, addr, flags, command, protocol_map.get(protocol, ''), read_write, res)


def parse_regulator_set_voltage_complete(data, one_event):
    name_pos = parse_int_field(one_event, "name", False) & 0xffff
    val = parse_int_field(one_event, "val", False)

    return "name=%s, val=%d" % (parse_bytes_to_str(data[name_pos:]), val)


def parse_regulator_set_voltage(data, one_event):
    name_pos = parse_int_field(one_event, "name", False) & 0xffff
    min_voltage = parse_int_field(one_event, "min", True)
    max_voltage = parse_int_field(one_event, "max", True)

    return "name=%s (%d-%d)" % (parse_bytes_to_str(data[name_pos:]), min_voltage, max_voltage)


def parse_regulator_funcs(data, one_event):
    name_pos = parse_int_field(one_event, "name", False) & 0xffff

    return "name=%s" % (parse_bytes_to_str(data[name_pos:]))


def parse_dma_fence_funcs(data, one_event):
    driver_pos = parse_int_field(one_event, "driver", False) & 0xffff
    timeline_pos = parse_int_field(one_event, "timeline", False) & 0xffff
    context = parse_int_field(one_event, "context", False)
    seqno = parse_int_field(one_event, "seqno", False)

    return "driver=%s timeline=%s context=%d seqno=%d" \
    % (parse_bytes_to_str(data[driver_pos:]), parse_bytes_to_str(data[timeline_pos:]), context, seqno)


def parse_binder_transaction(data, one_event):
    debug_id = parse_int_field(one_event, "debug_id", True)
    target_node = parse_int_field(one_event, "target_node", True)
    to_proc = parse_int_field(one_event, "to_proc", True)
    to_thread = parse_int_field(one_event, "to_thread", True)
    reply = parse_int_field(one_event, "reply", True)
    code = parse_int_field(one_event, "code", False)
    flags = parse_int_field(one_event, "flags", False)

    return "transaction=%d dest_node=%d dest_proc=%d dest_thread=%d reply=%d flags=0x%x code=0x%x" \
    % (debug_id, target_node, to_proc, to_thread, reply, flags, code)


def parse_binder_transaction_received(data, one_event):
    debug_id = parse_int_field(one_event, "debug_id", True)

    return "transaction=%d" % (debug_id)


def parse_mmc_request_start(data, one_event):
    cmd_opcode = parse_int_field(one_event, "cmd_opcode", False)
    cmd_arg = parse_int_field(one_event, "cmd_arg", False)
    cmd_flags = parse_int_field(one_event, "cmd_flags", False)
    cmd_retries = parse_int_field(one_event, "cmd_retries", False)
    stop_opcode = parse_int_field(one_event, "stop_opcode", False)
    stop_arg = parse_int_field(one_event, "stop_arg", False)
    stop_flags = parse_int_field(one_event, "stop_flags", False)
    stop_retries = parse_int_field(one_event, "stop_retries", False)
    sbc_opcode = parse_int_field(one_event, "sbc_opcode", False)
    sbc_arg = parse_int_field(one_event, "sbc_arg", False)
    sbc_flags = parse_int_field(one_event, "sbc_flags", False)
    sbc_retries = parse_int_field(one_event, "sbc_retries", False)
    blocks = parse_int_field(one_event, "blocks", False)
    blk_addr = parse_int_field(one_event, "blk_addr", False)
    blksz = parse_int_field(one_event, "blksz", False)
    data_flags = parse_int_field(one_event, "data_flags", False)
    tag = parse_int_field(one_event, "tag", True)
    can_retune = parse_int_field(one_event, "can_retune", False)
    doing_retune = parse_int_field(one_event, "doing_retune", False)
    retune_now = parse_int_field(one_event, "retune_now", False)
    need_retune = parse_int_field(one_event, "need_retune", False)
    hold_retune = parse_int_field(one_event, "hold_retune", True)
    retune_period = parse_int_field(one_event, "retune_period", True)
    mrq = parse_int_field(one_event, "mrq", False)
    name = parse_bytes_to_str(one_event["fields"]["name"])

    return "%s: start struct mmc_request[0x%x]: cmd_opcode=%d cmd_arg=0x%x cmd_flags=0x%x \
    cmd_retries=%d stop_opcode=%d stop_arg=0x%x stop_flags=0x%x stop_retries=%d sbc_opcode=%d \
    sbc_arg=0x%x sbc_flags=0x%x sbc_retires=%d blocks=%d block_size=%d blk_addr=%d data_flags=0x%x \
    tag=%d can_retune=%d doing_retune=%d retune_now=%d need_retune=%d hold_retune=%d retune_period=%d" \
    % (name, mrq, cmd_opcode, cmd_arg, cmd_flags, cmd_retries, stop_opcode, stop_arg, stop_flags, \
    stop_retries, sbc_opcode, sbc_arg, sbc_flags, sbc_retries, blocks, blksz, blk_addr, data_flags, \
    tag, can_retune, doing_retune, retune_now, need_retune, hold_retune, retune_period)


def parse_mmc_request_done(data, one_event):
    cmd_opcode = parse_int_field(one_event, "cmd_opcode", False)
    cmd_err = parse_int_field(one_event, "cmd_err", True)
    cmd_resp = list(one_event["fields"]["cmd_resp"])
    cmd_retries = parse_int_field(one_event, "cmd_retries", False)
    stop_opcode = parse_int_field(one_event, "stop_opcode", False)
    stop_err = parse_int_field(one_event, "stop_err", True)
    stop_resp = list(one_event["fields"]["stop_resp"])
    stop_retries = parse_int_field(one_event, "stop_retries", False)
    sbc_opcode = parse_int_field(one_event, "sbc_opcode", False)
    sbc_err = parse_int_field(one_event, "sbc_err", True)
    sbc_resp = list(one_event["fields"]["sbc_resp"])
    sbc_retries = parse_int_field(one_event, "sbc_retries", False)
    bytes_xfered = parse_int_field(one_event, "bytes_xfered", False)
    data_err = parse_int_field(one_event, "data_err", True)
    tag = parse_int_field(one_event, "tag", True)
    can_retune = parse_int_field(one_event, "can_retune", False)
    doing_retune = parse_int_field(one_event, "doing_retune", False)
    retune_now = parse_int_field(one_event, "retune_now", False)
    need_retune = parse_int_field(one_event, "need_retune", True)
    hold_retune = parse_int_field(one_event, "hold_retune", True)
    retune_period = parse_int_field(one_event, "retune_period", False)
    mrq = parse_int_field(one_event, "mrq", False)
    name = parse_bytes_to_str(one_event["fields"]["name"])

    return "%s: end struct mmc_request[0x%x]: cmd_opcode=%d cmd_err=%d cmd_resp=0x%x 0x%x 0x%x 0x%x \
    cmd_retries=%d stop_opcode=%d stop_err=%d stop_resp=0x%x 0x%x 0x%x 0x%x stop_retries=%d sbc_opcode=%d \
    sbc_err=%d sbc_resp=0x%x 0x%x 0x%x 0x%x sbc_retries=%d bytes_xfered=%d data_err=%d tag=%d can_retune=%d \
    doing_retune=%d retune_now=%d need_retune=%d hold_retune=%d retune_period=%d" \
    % (name, mrq, cmd_opcode, cmd_err, cmd_resp[0], cmd_resp[1], cmd_resp[2], cmd_resp[3], cmd_retries, \
    stop_opcode, stop_err, stop_resp[0], stop_resp[1], stop_resp[2], stop_resp[3], stop_retries, sbc_opcode, \
    sbc_err, sbc_resp[0], sbc_resp[1], sbc_resp[2], sbc_resp[3], sbc_retries, bytes_xfered, data_err, tag, \
    can_retune, doing_retune, retune_now, need_retune, hold_retune, retune_period)


def parse_file_check_and_advance_wb_err(data, one_event):
    file = parse_int_field(one_event, "file", False)
    i_ino = parse_int_field(one_event, "i_ino", False)
    s_dev = parse_int_field(one_event, "s_dev", False)
    old = parse_int_field(one_event, "old", False)
    new = parse_int_field(one_event, "new", False)

    return "file=0x%x dev=%d:%d ino=0x%x old=0x%x new=0x%x" \
    % (file, s_dev >> 20, s_dev & 0xfffff, i_ino, old, new)


def parse_filemap_set_wb_err(data, one_event):
    i_ino = parse_int_field(one_event, "i_ino", False)
    s_dev = parse_int_field(one_event, "s_dev", False)
    errseq = parse_int_field(one_event, "errseq", False)

    return "dev=%d:%d ino=0x%x errseq=0x%x" \
    % (s_dev >> 20, s_dev & 0xfffff, i_ino, errseq)


def parse_mm_filemap_add_or_delete_page_cache(data, one_event):
    pfn = parse_int_field(one_event, "pfn", False)
    i_ino = parse_int_field(one_event, "i_ino", False)
    index = parse_int_field(one_event, "index", False)
    s_dev = parse_int_field(one_event, "s_dev", False)
    pg = parse_int_field(one_event, "pg", False)

    return "dev %d:%d ino 0x%x page=0x%x pfn=%d ofs=%d" \
    % (s_dev >> 20, s_dev & 0xfffff, i_ino, pg, pfn, index << 12)


def parse_rss_stat(data, one_event):
    mm_id = parse_int_field(one_event, "mm_id", False)
    curr = parse_int_field(one_event, "curr", False)
    member = parse_int_field(one_event, "member", True)
    size = parse_int_field(one_event, "size", True)

    return "mm_id=%d curr=%d member=%d size=%d" % (mm_id, curr, member, size)


def parse_workqueue_execute_start_or_end(data, one_event):
    work = parse_int_field(one_event, "work", False)
    function = parse_int_field(one_event, "function", False)

    return "work struct 0x%x: function 0x%x" % (work, function)


def parse_thermal_power_allocator(data, one_event):
    tz_id = parse_int_field(one_event, "tz_id", True)
    req_power_pos = parse_int_field(one_event, "req_power", False) & 0xffff
    total_req_power = parse_int_field(one_event, "total_req_power", False)
    granted_power_pos = parse_int_field(one_event, "granted_power", False) & 0xffff
    total_granted_power = parse_int_field(one_event, "total_granted_power", False)
    num_actors = parse_int_field(one_event, "num_actors", False)
    power_range = parse_int_field(one_event, "power_range", False)
    max_allocatable_power = parse_int_field(one_event, "max_allocatable_power", False)
    current_temp = parse_int_field(one_event, "current_temp", True)
    delta_temp = parse_int_field(one_event, "delta_temp", True)

    req_power = "{" + ", ".join(str(x) for x in list(data[req_power_pos:req_power_pos + num_actors * 4])) + "}"
    granted_power = "{" + ", ".join(str(x) for x in list(data[granted_power_pos:granted_power_pos + num_actors * 4])) + "}"

    return "thermal_zone_id=%d req_power=%s total_req_power=%d granted_power=%s total_granted_power=%d \
    power_range=%d max_allocatable_power=%d current_temperature=%d delta_temperature=%d" \
    % (tz_id, req_power, total_req_power, granted_power, total_granted_power, power_range, \
    max_allocatable_power, current_temp, delta_temp)


def parse_thermal_power_allocator_pid(data, one_event):
    tz_id = parse_int_field(one_event, "tz_id", True)
    err = parse_int_field(one_event, "err", True)
    err_integral = parse_int_field(one_event, "err_integral", True)
    p = parse_int_field(one_event, "p", True)
    i = parse_int_field(one_event, "i", True)
    d = parse_int_field(one_event, "d", True)
    output = parse_int_field(one_event, "output", True)

    return "thermal_zone_id=%d err=%d err_integral=%d p=%d i=%d d=%d output=%d" \
    % (tz_id, err, err_integral, p, i, d, output)


def parse_print(data, one_event):
    ip = parse_int_field(one_event, "ip", False)
    buf_pos = 16

    return "0x%x: %s" % (ip, parse_bytes_to_str(data[buf_pos:]))


def parse_tracing_mark_write(data, one_event):
    data_pos = parse_int_field(one_event, "buffer", False) & 0xffff
    result_str = parse_bytes_to_str(data[data_pos:])
    if result_str.startswith("E|") and result_str[-1] == "|":
        result_str = result_str[:-1]
    elif result_str.startswith("S|") or result_str.startswith("F|") or result_str.startswith("C|"):
        pos = result_str.rfind(' ')
        result_str = result_str[:pos] + "|" + result_str[pos + 1:]
    return result_str


PRINT_FMT_SCHED_WAKEUP_HM = '"comm=%s pid=%d prio=%d target_cpu=%03d", REC->pname, REC->pid, REC->prio, REC->target_cpu'
PRINT_FMT_SCHED_WAKEUP = '"comm=%s pid=%d prio=%d target_cpu=%03d", REC->comm, REC->pid, REC->prio, REC->target_cpu'
PRINT_FMT_SCHED_SWITCH_HM = '"prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s" " ==> next_comm=%s next_pid=%d next_prio=%d", REC->pname, REC->prev_tid, REC->pprio, hm_trace_tcb_state2str(REC->pstate), REC->nname, REC->next_tid, REC->nprio'
PRINT_FMT_SCHED_SWITCH = '"prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d expeller_type=%u", REC->prev_comm, REC->prev_pid, REC->prev_prio, (REC->prev_state & ((((0x0000 | 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0040) + 1) << 1) - 1)) ? __print_flags(REC->prev_state & ((((0x0000 | 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0040) + 1) << 1) - 1), "|", { 0x0001, "S" }, { 0x0002, "D" }, { 0x0004, "T" }, { 0x0008, "t" }, { 0x0010, "X" }, { 0x0020, "Z" }, { 0x0040, "P" }, { 0x0080, "I" }) : "R", REC->prev_state & (((0x0000 | 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0040) + 1) << 1) ? "+" : "", REC->next_comm, REC->next_pid, REC->next_prio, REC->expeller_type'
PRINT_FMT_SCHED_BLOCKED_REASON_HM = '"pid=%d iowait=%d caller=%s delay=%llu", REC->pid, REC->iowait, hmtrace_sched_blocked_reason_of(REC->cnode_idx, REC->caller), REC->delay >> 10'
PRINT_FMT_SCHED_BLOCKED_REASON = '"pid=%d iowait=%d caller=%pS delay=%lu", REC->pid, REC->io_wait, REC->caller, REC->delay>>10'
PRINT_FMT_CPU_FREQUENCY_HM = '"state=%u cpu_id=%u", REC->state, REC->cpu_id'
PRINT_FMT_CPU_FREQUENCY = '"state=%lu cpu_id=%lu", (unsigned long)REC->state, (unsigned long)REC->cpu_id'
PRINT_FMT_CLOCK_SET_RATE_HM = '"%s state=%lu cpu_id=%lu", ((char *)((void *)((char *)REC + (REC->__data_loc_name & 0xffff)))), (unsigned long)REC->state, (unsigned long)REC->cpu_id'
PRINT_FMT_CLOCK_SET_RATE = '"%s state=%lu cpu_id=%lu", __get_str(name), (unsigned long)REC->state, (unsigned long)REC->cpu_id'
PRINT_FMT_CPU_FREQUENCY_LIMITS_HM = '"min=%lu max=%lu cpu_id=%lu", (unsigned long)REC->min, (unsigned long)REC->max, (unsigned long)REC->cpu_id'
PRINT_FMT_CPU_FREQUENCY_LIMITS = '"min=%lu max=%lu cpu_id=%lu", (unsigned long)REC->min_freq, (unsigned long)REC->max_freq, (unsigned long)REC->cpu_id'
PRINT_FMT_CPU_IDLE_HM = '"state=%u cpu_id=%u", REC->state, REC->cpu_id'
PRINT_FMT_CPU_IDLE = '"state=%lu cpu_id=%lu", (unsigned long)REC->state, (unsigned long)REC->cpu_id'
PRINT_FMT_EXT4_DA_WRITE_BEGIN = '"dev %d,%d ino %lu pos %lld len %u flags %u", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), (unsigned long) REC->ino, REC->pos, REC->len, REC->flags'
PRINT_FMT_EXT4_DA_WRITE_END = '"dev %d,%d ino %lu pos %lld len %u copied %u", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), (unsigned long) REC->ino, REC->pos, REC->len, REC->copied'
PRINT_FMT_EXT4_SYNC_FILE_ENTER = '"dev %d,%d ino %lu parent %lu datasync %d ", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), (unsigned long) REC->ino, (unsigned long) REC->parent, REC->datasync'
PRINT_FMT_EXT4_SYNC_FILE_EXIT = '"dev %d,%d ino %lu ret %d", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), (unsigned long) REC->ino, REC->ret'
PRINT_FMT_BLOCK_BIO_REMAP = '"%d,%d %s %llu + %u <- (%d,%d) %llu", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), REC->rwbs, (unsigned long long)REC->sector, REC->nr_sector, ((unsigned int) ((REC->old_dev) >> 20)), ((unsigned int) ((REC->old_dev) & ((1U << 20) - 1))), (unsigned long long)REC->old_sector'
PRINT_FMT_BLOCK_RQ_ISSUE_HM = '"%d,%d %s %u (%s) %llu + %u [%s]", ((unsigned int) ((REC->dev) >> 20U)), ((unsigned int) ((REC->dev) & ((1U << 20U) - 1U))), REC->rwbs, REC->bytes, REC->cmd, (unsigned long long)REC->sector, REC->nr_sector, REC->comm'
PRINT_FMT_BLOCK_RQ_ISSUE_OR_INSERT = '"%d,%d %s %u (%s) %llu + %u [%s]", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), REC->rwbs, REC->bytes, __get_str(cmd), (unsigned long long)REC->sector, REC->nr_sector, REC->comm'
PRINT_FMT_BLOCK_RQ_COMPLETE_HM = '"%d,%d %s (%s) %llu + %u [%d]", ((unsigned int) ((REC->dev) >> 20U)), ((unsigned int) ((REC->dev) & ((1U << 20U) - 1U))), REC->rwbs, REC->cmd, (unsigned long long)REC->sector, REC->nr_sector, REC->error'
PRINT_FMT_BLOCK_RQ_COMPLETE = '"%d,%d %s (%s) %llu + %u [%d]", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), REC->rwbs, __get_str(cmd), (unsigned long long)REC->sector, REC->nr_sector, REC->error'
PRINT_FMT_UFSHCD_COMMAND_HM = '"%s: %s: tag: %u, DB: 0x%x, size: %d, IS: %u, LBA: %llu, opcode: 0x%x", REC->str, REC->dev_name, REC->tag, REC->doorbell, REC->transfer_len, REC->intr, REC->lba, (uint32_t)REC->opcode'
PRINT_FMT_UFSHCD_COMMAND = '"%s: %s: tag: %u, DB: 0x%x, size: %d, IS: %u, LBA: %llu, opcode: 0x%x (%s), group_id: 0x%x", __get_str(str), __get_str(dev_name), REC->tag, REC->doorbell, REC->transfer_len, REC->intr, REC->lba, (u32)REC->opcode, __print_symbolic(REC->opcode, { 0x8a, "WRITE_16" }, { 0x2a, "WRITE_10" }, { 0x88, "READ_16" }, { 0x28, "READ_10" }, { 0x35, "SYNC" }, { 0x42, "UNMAP" }), (u32)REC->group_id'
PRINT_FMT_UFSHCD_UPIU = '"%s: %s: HDR:%s, CDB:%s", __get_str(str), __get_str(dev_name), __print_hex(REC->hdr, sizeof(REC->hdr)), __print_hex(REC->tsf, sizeof(REC->tsf))'
PRINT_FMT_UFSHCD_UIC_COMMAND = '"%s: %s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x", __get_str(str), __get_str(dev_name), REC->cmd, REC->arg1, REC->arg2, REC->arg3'
PRINT_FMT_UFSHCD_FUNCS = '"%s: took %lld usecs, dev_state: %s, link_state: %s, err %d", __get_str(dev_name), REC->usecs, __print_symbolic(REC->dev_state, { 1, "UFS_ACTIVE_PWR_MODE" }, { 2, "UFS_SLEEP_PWR_MODE" }, { 3, "UFS_POWERDOWN_PWR_MODE" }), __print_symbolic(REC->link_state, { 0, "UIC_LINK_OFF_STATE" }, { 1, "UIC_LINK_ACTIVE_STATE" }, { 2, "UIC_LINK_HIBERN8_STATE" }), REC->err'
PRINT_FMT_UFSHCD_PROFILE_FUNCS = '"%s: %s: took %lld usecs, err %d", __get_str(dev_name), __get_str(profile_info), REC->time_us, REC->err'
PRINT_FMT_UFSHCD_AUTO_BKOPS_STATE = '"%s: auto bkops - %s", __get_str(dev_name), __get_str(state)'
PRINT_FMT_UFSHCD_CLK_SCALING = '"%s: %s %s from %u to %u Hz", __get_str(dev_name), __get_str(state), __get_str(clk), REC->prev_state, REC->curr_state'
PRINT_FMT_UFSHCD_CLK_GATING = '"%s: gating state changed to %s", __get_str(dev_name), __print_symbolic(REC->state, { 0, "CLKS_OFF" }, { 1, "CLKS_ON" }, { 2, "REQ_CLKS_OFF" }, { 3, "REQ_CLKS_ON" })'
PRINT_FMT_I2C_READ = '"i2c-%d #%u a=%03x f=%04x l=%u", REC->adapter_nr, REC->msg_nr, REC->addr, REC->flags, REC->len'
PRINT_FMT_I2C_WRITE_OR_REPLY = '"i2c-%d #%u a=%03x f=%04x l=%u [%*phD]", REC->adapter_nr, REC->msg_nr, REC->addr, REC->flags, REC->len, REC->len, __get_dynamic_array(buf)'
PRINT_FMT_I2C_RESULT = '"i2c-%d n=%u ret=%d", REC->adapter_nr, REC->nr_msgs, REC->ret'
PRINT_FMT_SMBUS_READ = '"i2c-%d a=%03x f=%04x c=%x %s", REC->adapter_nr, REC->addr, REC->flags, REC->command, __print_symbolic(REC->protocol, { 0, "QUICK" }, { 1, "BYTE" }, { 2, "BYTE_DATA" }, { 3, "WORD_DATA" }, { 4, "PROC_CALL" }, { 5, "BLOCK_DATA" }, { 6, "I2C_BLOCK_BROKEN" }, { 7, "BLOCK_PROC_CALL" }, { 8, "I2C_BLOCK_DATA" })'
PRINT_FMT_SMBUS_WRITE_OR_REPLY = '"i2c-%d a=%03x f=%04x c=%x %s l=%u [%*phD]", REC->adapter_nr, REC->addr, REC->flags, REC->command, __print_symbolic(REC->protocol, { 0, "QUICK" }, { 1, "BYTE" }, { 2, "BYTE_DATA" }, { 3, "WORD_DATA" }, { 4, "PROC_CALL" }, { 5, "BLOCK_DATA" }, { 6, "I2C_BLOCK_BROKEN" }, { 7, "BLOCK_PROC_CALL" }, { 8, "I2C_BLOCK_DATA" }), REC->len, REC->len, REC->buf'
PRINT_FMT_SMBUS_RESULT = '"i2c-%d a=%03x f=%04x c=%x %s %s res=%d", REC->adapter_nr, REC->addr, REC->flags, REC->command, __print_symbolic(REC->protocol, { 0, "QUICK" }, { 1, "BYTE" }, { 2, "BYTE_DATA" }, { 3, "WORD_DATA" }, { 4, "PROC_CALL" }, { 5, "BLOCK_DATA" }, { 6, "I2C_BLOCK_BROKEN" }, { 7, "BLOCK_PROC_CALL" }, { 8, "I2C_BLOCK_DATA" }), REC->read_write == 0 ? "wr" : "rd", REC->res'
PRINT_FMT_REGULATOR_SET_VOLTAGE_COMPLETE = '"name=%s, val=%u", __get_str(name), (int)REC->val'
PRINT_FMT_REGULATOR_SET_VOLTAGE = '"name=%s (%d-%d)", __get_str(name), (int)REC->min, (int)REC->max'
PRINT_FMT_REGULATOR_FUNCS = '"name=%s", __get_str(name)'
PRINT_FMT_DMA_FENCE_FUNCS = '"driver=%s timeline=%s context=%u seqno=%u", __get_str(driver), __get_str(timeline), REC->context, REC->seqno'
PRINT_FMT_BINDER_TRANSACTION = '"transaction=%d dest_node=%d dest_proc=%d dest_thread=%d reply=%d flags=0x%x code=0x%x", REC->debug_id, REC->target_node, REC->to_proc, REC->to_thread, REC->reply, REC->flags, REC->code'
PRINT_FMT_BINDER_TRANSACTION_RECEIVED = '"transaction=%d", REC->debug_id'
PRINT_FMT_MMC_REQUEST_START = '"%s: start struct mmc_request[%p]: cmd_opcode=%u cmd_arg=0x%x cmd_flags=0x%x cmd_retries=%u stop_opcode=%u stop_arg=0x%x stop_flags=0x%x stop_retries=%u sbc_opcode=%u sbc_arg=0x%x sbc_flags=0x%x sbc_retires=%u blocks=%u block_size=%u blk_addr=%u data_flags=0x%x tag=%d can_retune=%u doing_retune=%u retune_now=%u need_retune=%d hold_retune=%d retune_period=%u", __get_str(name), REC->mrq, REC->cmd_opcode, REC->cmd_arg, REC->cmd_flags, REC->cmd_retries, REC->stop_opcode, REC->stop_arg, REC->stop_flags, REC->stop_retries, REC->sbc_opcode, REC->sbc_arg, REC->sbc_flags, REC->sbc_retries, REC->blocks, REC->blksz, REC->blk_addr, REC->data_flags, REC->tag, REC->can_retune, REC->doing_retune, REC->retune_now, REC->need_retune, REC->hold_retune, REC->retune_period'
PRINT_FMT_MMC_REQUEST_DONE = '"%s: end struct mmc_request[%p]: cmd_opcode=%u cmd_err=%d cmd_resp=0x%x 0x%x 0x%x 0x%x cmd_retries=%u stop_opcode=%u stop_err=%d stop_resp=0x%x 0x%x 0x%x 0x%x stop_retries=%u sbc_opcode=%u sbc_err=%d sbc_resp=0x%x 0x%x 0x%x 0x%x sbc_retries=%u bytes_xfered=%u data_err=%d tag=%d can_retune=%u doing_retune=%u retune_now=%u need_retune=%d hold_retune=%d retune_period=%u", __get_str(name), REC->mrq, REC->cmd_opcode, REC->cmd_err, REC->cmd_resp[0], REC->cmd_resp[1], REC->cmd_resp[2], REC->cmd_resp[3], REC->cmd_retries, REC->stop_opcode, REC->stop_err, REC->stop_resp[0], REC->stop_resp[1], REC->stop_resp[2], REC->stop_resp[3], REC->stop_retries, REC->sbc_opcode, REC->sbc_err, REC->sbc_resp[0], REC->sbc_resp[1], REC->sbc_resp[2], REC->sbc_resp[3], REC->sbc_retries, REC->bytes_xfered, REC->data_err, REC->tag, REC->can_retune, REC->doing_retune, REC->retune_now, REC->need_retune, REC->hold_retune, REC->retune_period'
PRINT_FMT_FILE_CHECK_AND_ADVANCE_WB_ERR = '"file=%p dev=%d:%d ino=0x%lx old=0x%x new=0x%x", REC->file, ((unsigned int)((REC->s_dev) >> 20)), ((unsigned int)((REC->s_dev) & ((1U << 20) - 1))), REC->i_ino, REC->old, REC->new'
PRINT_FMT_FILEMAP_SET_WB_ERR = '"dev=%d:%d ino=0x%lx errseq=0x%x", ((unsigned int)((REC->s_dev) >> 20)), ((unsigned int)((REC->s_dev) & ((1U << 20) - 1))), REC->i_ino, REC->errseq'
PRINT_FMT_MM_FILEMAP_ADD_OR_DELETE_PAGE_CACHE = '"dev %d:%d ino %lx page=%px pfn=%lu ofs=%lu", ((unsigned int)((REC->s_dev) >> 20)), ((unsigned int)((REC->s_dev) & ((1U << 20) - 1))), REC->i_ino, REC->pg, REC->pfn, REC->index << 12'
PRINT_FMT_RSS_STAT_HM = '"mm_id=%u curr=%d member=%d size=%ldB", REC->mm_id, REC->curr, REC->member, REC->size'
PRINT_FMT_WORKQUEUE_EXECUTE_START_OR_END = '"work struct %p: function %ps", REC->work, REC->function'
PRINT_FMT_THERMAL_POWER_ALLOCATOR_PID = '"thermal_zone_id=%d err=%d err_integral=%d p=%lld i=%lld d=%lld output=%d", REC->tz_id, REC->err, REC->err_integral, REC->p, REC->i, REC->d, REC->output'
PRINT_FMT_THERMAL_POWER_ALLOCATOR = '"thermal_zone_id=%d req_power={%s} total_req_power=%u granted_power={%s} total_granted_power=%u power_range=%u max_allocatable_power=%u current_temperature=%d delta_temperature=%d", REC->tz_id, __print_array(__get_dynamic_array(req_power), REC->num_actors, 4), REC->total_req_power, __print_array(__get_dynamic_array(granted_power), REC->num_actors, 4), REC->total_granted_power, REC->power_range, REC->max_allocatable_power, REC->current_temp, REC->delta_temp'
PRINT_FMT_PRINT = '"%ps: %s", (void *)REC->ip, REC->buf'
PRINT_FMT_TRACING_MARK_WRITE = '"%s", ((void *)((char *)REC + (REC->__data_loc_buffer & 0xffff)))'


print_fmt_func_map = {
PRINT_FMT_SCHED_WAKEUP_HM: parse_sched_wakeup_hm,
PRINT_FMT_SCHED_WAKEUP: parse_sched_wakeup,
PRINT_FMT_SCHED_SWITCH_HM: parse_sched_switch_hm,
PRINT_FMT_SCHED_SWITCH: parse_sched_switch,
PRINT_FMT_SCHED_BLOCKED_REASON_HM: parse_sched_blocked_reason_hm,
PRINT_FMT_SCHED_BLOCKED_REASON: parse_sched_blocked_reason,
PRINT_FMT_CPU_FREQUENCY_HM: parse_cpu_frequency,
PRINT_FMT_CPU_FREQUENCY: parse_cpu_frequency,
PRINT_FMT_CLOCK_SET_RATE_HM: parse_clock_set_rate,
PRINT_FMT_CLOCK_SET_RATE: parse_clock_set_rate,
PRINT_FMT_CPU_FREQUENCY_LIMITS_HM: parse_cpu_frequency_limits_hm,
PRINT_FMT_CPU_FREQUENCY_LIMITS: parse_cpu_frequency_limits,
PRINT_FMT_CPU_IDLE_HM: parse_cpu_idle,
PRINT_FMT_CPU_IDLE: parse_cpu_idle,
PRINT_FMT_EXT4_DA_WRITE_BEGIN: parse_ext4_da_write_begin,
PRINT_FMT_EXT4_DA_WRITE_END: parse_ext4_da_write_end,
PRINT_FMT_EXT4_SYNC_FILE_ENTER: parse_ext4_sync_file_enter,
PRINT_FMT_EXT4_SYNC_FILE_EXIT: parse_ext4_sync_file_exit,
PRINT_FMT_BLOCK_BIO_REMAP: parse_block_bio_remap,
PRINT_FMT_BLOCK_RQ_ISSUE_HM: parse_block_rq_issue_hm,
PRINT_FMT_BLOCK_RQ_ISSUE_OR_INSERT: parse_block_rq_issue_or_insert,
PRINT_FMT_BLOCK_RQ_COMPLETE_HM: parse_block_rq_complete_hm,
PRINT_FMT_BLOCK_RQ_COMPLETE: parse_block_rq_complete,
PRINT_FMT_UFSHCD_COMMAND_HM: parse_ufshcd_command_hm,
PRINT_FMT_UFSHCD_COMMAND: parse_ufshcd_command,
PRINT_FMT_UFSHCD_UPIU: parse_ufshcd_upiu,
PRINT_FMT_UFSHCD_UIC_COMMAND: parse_ufshcd_uic_command,
PRINT_FMT_UFSHCD_FUNCS: parse_ufshcd_funcs,
PRINT_FMT_UFSHCD_PROFILE_FUNCS: parse_ufshcd_profile_funcs,
PRINT_FMT_UFSHCD_AUTO_BKOPS_STATE: parse_ufshcd_auto_bkops_state,
PRINT_FMT_UFSHCD_CLK_SCALING: parse_ufshcd_clk_scaling,
PRINT_FMT_UFSHCD_CLK_GATING: parse_ufshcd_clk_gating,
PRINT_FMT_I2C_READ: parse_i2c_read,
PRINT_FMT_I2C_WRITE_OR_REPLY: parse_i2c_write_or_reply,
PRINT_FMT_I2C_RESULT: parse_i2c_result,
PRINT_FMT_SMBUS_READ: parse_smbus_read,
PRINT_FMT_SMBUS_WRITE_OR_REPLY: parse_smbus_write_or_reply,
PRINT_FMT_SMBUS_RESULT: parse_smbus_result,
PRINT_FMT_REGULATOR_SET_VOLTAGE_COMPLETE: parse_regulator_set_voltage_complete,
PRINT_FMT_REGULATOR_SET_VOLTAGE: parse_regulator_set_voltage,
PRINT_FMT_REGULATOR_FUNCS: parse_regulator_funcs,
PRINT_FMT_DMA_FENCE_FUNCS: parse_dma_fence_funcs,
PRINT_FMT_BINDER_TRANSACTION: parse_binder_transaction,
PRINT_FMT_BINDER_TRANSACTION_RECEIVED: parse_binder_transaction_received,
PRINT_FMT_MMC_REQUEST_START: parse_mmc_request_start,
PRINT_FMT_MMC_REQUEST_DONE: parse_mmc_request_done,
PRINT_FMT_FILE_CHECK_AND_ADVANCE_WB_ERR: parse_file_check_and_advance_wb_err,
PRINT_FMT_FILEMAP_SET_WB_ERR: parse_filemap_set_wb_err,
PRINT_FMT_MM_FILEMAP_ADD_OR_DELETE_PAGE_CACHE: parse_mm_filemap_add_or_delete_page_cache,
PRINT_FMT_RSS_STAT_HM: parse_rss_stat,
PRINT_FMT_WORKQUEUE_EXECUTE_START_OR_END: parse_workqueue_execute_start_or_end,
PRINT_FMT_THERMAL_POWER_ALLOCATOR_PID: parse_thermal_power_allocator_pid,
PRINT_FMT_THERMAL_POWER_ALLOCATOR: parse_thermal_power_allocator,
PRINT_FMT_PRINT: parse_print,
PRINT_FMT_TRACING_MARK_WRITE: parse_tracing_mark_write
}
