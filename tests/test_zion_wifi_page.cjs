/* Run the exact embedded script against a small DOM/network fixture. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const header = fs.readFileSync('main/zion_wifi_page.h', 'utf8');
const source = header.split('static const char wifi_script[] =')[1].trim()
    .split('\n').map(x => JSON.parse(x.trim().replace(/;$/, ''))).join('');
const hex = name => Buffer.from(name).toString('hex');
async function fixture() {
    const nodes = new Map(); let scans = 0, polls = 0, fail = false;
    function node() {
        return {value:'', textContent:'', disabled:false, required:false, hidden:false, children:[],
            replaceChildren() {this.children=[]; this.value='';},
            append(o) {this.children.push(o);if(this.children.length===1)this.value=o.value;}};
    }
    const get = id => {if(!nodes.has(id))nodes.set(id,node());return nodes.get(id);};
    const hostile = '<img src=x onerror=alert(1)>"&';
    vm.runInNewContext(source, {
        document:{getElementById:get,createElement:node},TextDecoder,Uint8Array,
        setTimeout:callback=>setImmediate(callback),
        fetch:async (url,options={}) => {
            if(url==='/token')return {ok:true,text:async ()=>'0123456789abcdef'};
            assert.equal(url,'/scan');
            if(options.method==='POST') {
                scans++;assert.equal(options.headers['X-Zion-Token'],'0123456789abcdef');
                return {ok:!fail,status:fail?409:200};
            }
            polls++;return {ok:true,json:async()=>({pending:false,error:0,networks:[
                {hex:hex(hostile),rssi:-40,secure:true},
                {hex:hex(hostile),rssi:-50,secure:true},
                {hex:hex('中文开放网络'),rssi:-60,secure:false},
                {hex:'',rssi:-70,secure:false}]})};
        }
    });
    async function idle() {
        for(let i=0;i<100;i++) {await new Promise(setImmediate);if(scans && !get('scan').disabled)return;}
        throw Error('scan did not finish');
    }
    await idle();assert.equal(scans,1);assert.equal(polls,1);
    assert.equal(get('ssid').children.length,2);assert.equal(get('ssid').value,hostile);
    assert.ok(get('ssid').children[0].textContent.startsWith(hostile));
    assert.equal(get('password').required,true);assert.equal(get('save').disabled,false);
    get('ssid').value='中文开放网络';get('password').value='previousSecret';get('ssid').onchange();
    assert.equal(get('passwordLabel').hidden,true);assert.equal(get('password').value,'');
    assert.equal(get('password').required,false);assert.equal(get('password').disabled,false);
    const first=get('scan').onclick();await get('scan').onclick();await first;await idle();
    assert.equal(scans,2);assert.equal(get('ssid').value,'中文开放网络');
    fail=true;await get('scan').onclick();assert.match(get('status').textContent,/设备忙/);
    assert.equal(get('scan').disabled,false);assert.equal(get('ssid').value,'中文开放网络');
    fail=false;await get('scan').onclick();assert.equal(scans,4);
    console.log('Zion Wi-Fi page: PASS (auto/rescan, dedup, UTF-8, inert HTML, open/encrypted, busy retry)');
}
fixture().catch(e=>{console.error(e);process.exitCode=1;});
