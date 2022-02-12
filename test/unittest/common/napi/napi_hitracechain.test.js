/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

import hiTraceChain from "@ohos.hiTraceChain"
import hiTraceMeter from "@ohos.hiTraceMeter"

import {describe, beforeAll, beforeEach, afterEach, afterAll, it, expect} from 'deccjsunit/index'

describe('hiTraceChainJsUnitTest', function () {
    beforeAll(function() {
        /*
         * @tc.setup: setup invoked before all test cases
         */
        console.info('hiTraceChainJsUnitTest beforeAll called')
    })

    afterAll(function() {
        /*
         * @tc.teardown: teardown invoked after all test cases
         */
        console.info('hiTraceChainJsUnitTest afterAll called')
    })

    beforeEach(function() {
        /*
         * @tc.setup: setup invoked before each test case
         */
        console.info('hiTraceChainJsUnitTest beforeEach called')
    })

    afterEach(function() {
        /*
         * @tc.teardown: teardown invoked after each test case
         */
        console.info('hiTraceChainJsUnitTest afterEach called')
    })

    /**
     * @tc.name: hiTraceChainJsUnitTest001
     * @tc.desc: 
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest001', 0, async function (done) {
        console.info('hiTraceChainJsUnitTest001 start');
        let traceid = hiTraceChain.begin("hiTraceChainJsUnitTest001");
        console.info('hiTraceChainJsUnitTest001 >> traceid.chainId is ' + traceid.chainId);
        hiTraceMeter.startTrace("hiTraceChainJsUnitTest001", 199);
        console.info('hiTraceChainJsUnitTest001 end');
    });
});