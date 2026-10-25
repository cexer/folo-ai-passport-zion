const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const header = fs.readFileSync('main/zion_wifi_result.h','utf8');
const script = header.split('static const char wifi_result_script[] =')[1].trim()
    .split('\n').map(line=>JSON.parse(line.trim().replace(/;$/,''))).join('');
const entry = /#define ZION_MINI_PROGRAM_URL "([^"]+)"/.exec(fs.readFileSync('main/zion_portal.h','utf8'))[1];
assert.match(entry,/^weixin:\/\/dl\/business\/\?appid=wxd2a90fe8e2accc23&path=pages\/index\/index&env_version=release$/);
assert.ok(header.includes('src=/mini-code.png'));
async function run(states,failPath) {
    const nodes=new Map(),calls=[],navigations=[];
    const get=id=>{if(!nodes.has(id))nodes.set(id,{textContent:'',hidden:id==='success',href:id==='open'?entry:'/'});return nodes.get(id);};
    let at=0;
    const value=vm.runInNewContext(script,{
        document:{getElementById:get},
        location:{set href(url){navigations.push(url);}},
        setTimeout:cb=>setImmediate(cb),
        fetch:async(path,options={})=>{
            calls.push(path);
            const ok=path!==failPath;
            if(path==='/token')return {ok,text:async()=>'0123456789abcdef'};
            if(path==='/status')return {ok,json:async()=>states[Math.min(at++,states.length-1)]};
            assert.ok(path==='/apply'||path==='/finish');
            assert.equal(options.method,'POST');assert.equal(options.body,'token=0123456789abcdef');
            return {ok};
        }
    });
    await value;await new Promise(setImmediate);
    return {get,calls,navigations};
}
(async()=>{
    let r=await run([{state:0,pending:true},{state:1},{state:2,error:0}]);
    assert.deepEqual(r.calls,['/token','/status','/apply','/status','/status','/finish']);
    assert.deepEqual(r.navigations,[entry]);assert.equal(r.get('success').hidden,false);
    assert.equal(r.get('retry').hidden,true);assert.equal(r.get('title').textContent,'配网成功');
    r=await run([{state:2,error:0}]);assert.ok(!r.calls.includes('/apply'));assert.deepEqual(r.navigations,[entry]);
    r=await run([{state:1},{state:3,error:263}]);
    assert.equal(r.get('success').hidden,true);assert.match(r.get('status').textContent,/263/);
    assert.ok(!r.calls.includes('/finish'));assert.equal(r.navigations.length,0);
    r=await run([{state:0,pending:false}]);assert.match(r.get('status').textContent,/没有待连接/);
    r=await run([{state:0,pending:true}],'/apply');assert.match(r.get('status').textContent,/设备忙/);assert.equal(r.navigations.length,0);
    r=await run([{state:1}],'/status');assert.match(r.get('status').textContent,/无法读取/);
    r=await run([{state:1}]);assert.match(r.get('status').textContent,/超时/);assert.equal(r.navigations.length,0);
    r=await run([{state:2}],'/finish');assert.deepEqual(r.navigations,[entry]);
    console.log('Zion Wi-Fi result: PASS (poll/refresh, success-only handoff, actual official entry, failures, timeout, QR fallback)');
})().catch(e=>{console.error(e);process.exitCode=1;});
