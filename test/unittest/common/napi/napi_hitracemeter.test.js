/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import hiTraceMeter from '@ohos.hiTraceMeter';

import {describe, beforeAll, beforeEach, afterEach, afterAll, it, expect} from 'deccjsunit/index'

describe('hiTraceMeterJsUnitTest', function () {
    let logTag = '[hiTraceMeterJsUnitTest]';
    const DEBUG = hiTraceMeter.HiTraceOutputLevel.DEBUG;
    const INFO = hiTraceMeter.HiTraceOutputLevel.INFO;
    const CRITICAL = hiTraceMeter.HiTraceOutputLevel.CRITICAL;
    const COMMERCIAL = hiTraceMeter.HiTraceOutputLevel.COMMERCIAL;
    const MAX = hiTraceMeter.HiTraceOutputLevel.MAX;

    beforeAll(function() {

        /**
         * @tc.setup: setup invoked before all test cases
         */
        console.info(logTag, 'hiTraceMeterJsUnitTest beforeAll called');
    })

    afterAll(function() {

        /**
         * @tc.teardown: teardown invoked after all test cases
         */
        console.info(logTag, 'hiTraceMeterJsUnitTest afterAll called');
    })

    beforeEach(function() {

        /**
         * @tc.setup: setup invoked before each test case
         */
        console.info(logTag, 'hiTraceMeterJsUnitTest beforeEach called');
    })

    afterEach(function() {

        /**
         * @tc.teardown: teardown invoked after each test case
         */
        console.info(logTag, 'hiTraceMeterJsUnitTest afterEach called');
    })

    /**
     * @tc.name: hiTraceMeterJsUnitTest001
     * @tc.desc: test hiTraceMeter.startTrace/hiTraceMeter.finishTrace
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest001', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest001: start.');
        let name = 'hiTraceMeterJsUnitTest001';
        let taskId = 1001;
        try {
            hiTraceMeter.startTrace(name, taskId);
            hiTraceMeter.finishTrace(name, taskId);
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest001: end.');
    });

    /**
     * @tc.name: hiTraceMeterJsUnitTest002
     * @tc.desc: test hiTraceMeter.traceByValue
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest002', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest002: start.');
        let name = 'hiTraceMeterJsUnitTest002';
        let count = 1002;
        try {
            hiTraceMeter.traceByValue(name, count);
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest002: end.');
    });

    /**
     * @tc.name: hiTraceMeterJsUnitTest003
     * @tc.desc: test hiTraceMeter.startSyncTrace/hiTraceMeter.finishSyncTrace with different level
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest003', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest003: start.');
        let name = 'hiTraceMeterJsUnitTest003';
        let customArgs = 'key=value';
        try {
            hiTraceMeter.startSyncTrace(DEBUG, name, customArgs);
            hiTraceMeter.finishSyncTrace(DEBUG, name, customArgs);
            hiTraceMeter.startSyncTrace(INFO, name, customArgs);
            hiTraceMeter.finishSyncTrace(INFO, name, customArgs);
            hiTraceMeter.startSyncTrace(CRITICAL, name, customArgs);
            hiTraceMeter.finishSyncTrace(CRITICAL, name, customArgs);
            hiTraceMeter.startSyncTrace(COMMERCIAL, name, customArgs);
            hiTraceMeter.finishSyncTrace(COMMERCIAL, name, customArgs);
            hiTraceMeter.startSyncTrace(MAX, name, customArgs);
            hiTraceMeter.finishSyncTrace(MAX, name, customArgs);
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest003: end.');
    });

    /**
     * @tc.name: hiTraceMeterJsUnitTest004
     * @tc.desc: test hiTraceMeter.startAsyncTrace/hiTraceMeter.finishAsyncTrace
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest004', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest004: start.');
        let name = 'hiTraceMeterJsUnitTest004';
        let customCategory = 'test';
        let customArgs = 'key=value';
        let taskId = 1004;
        try {
            hiTraceMeter.startAsyncTrace(COMMERCIAL, name, taskId, customCategory, customArgs);
            hiTraceMeter.finishAsyncTrace(COMMERCIAL, name, taskId);
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest004: end.');
    });

    /**
     * @tc.name: hiTraceMeterJsUnitTest005
     * @tc.desc: test hiTraceMeter.traceByValue with level
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest005', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest005: start.');
        let name = 'hiTraceMeterJsUnitTest005';
        let count = 1005;
        try {
            hiTraceMeter.traceByValue(COMMERCIAL, name, count);
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest005: end.');
    });

    /**
     * @tc.name: hiTraceMeterJsUnitTest006
     * @tc.desc: test isTraceEnabled
     * @tc.type: FUNC
     */
    it('hiTraceMeterJsUnitTest006', 0, async function (done) {
        console.info(logTag, 'hiTraceMeterJsUnitTest006: start.');
        let name = 'hiTraceMeterJsUnitTest006';
        try {
            if (hiTraceMeter.isTraceEnabled()) {
                console.info(logTag, 'trace enable.');
            } else {
                console.info(logTag, 'trace disable.');
            }
            expect(true).assertTrue();
        } catch (err) {
            console.info(logTag, name + 'fail, err = ' + err);
            expect(false).assertTrue();
        }
        done();
        console.info(logTag, 'hiTraceMeterJsUnitTest006: end.');
    });
});
