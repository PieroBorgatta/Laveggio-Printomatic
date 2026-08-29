import { createServer as createHttpServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, extname, join, normalize } from 'node:path';

const here=dirname(fileURLToPath(import.meta.url));
const dataRoot=join(here,'..','data');

const defaultSettings={device_id:'laveggio-printomatic-01',hostname:'laveggio-pesa',wifi_ssid:'Distilleria',use_dhcp:false,static_ip:'192.168.0.42',gateway:'192.168.0.1',subnet:'255.255.255.0',dns:'192.168.0.1',backend_url:'',tls_ca_certificate:'',notification_url:'',stable_ms:600,heartbeat_seconds:30,ntp_server:'pool.ntp.org',timezone:'CET-1CEST,M3.5.0,M10.5.0/3',admin_user:'admin',display_default_on:false,power_sense_enabled:true};
const multipliers=[10000,1000,100,10];

function initialState(){return {displayOn:false,settings:{...defaultSettings},sequence:1842,calibration:{channels:multipliers.map((multiplier,channel)=>({channel,multiplier_kg:multiplier,tolerance:90,hysteresis:28,points:Array.from({length:10},(_,position)=>({position,enabled:position<4,raw:(channel*733+position*405+180)%4096}))}))},history:Array.from({length:12},(_,index)=>({type:'scale.snapshot',captured_at:new Date(Date.now()-index*840000).toISOString(),weight_kg:12340-index*10,digits:[1,2,3,(4-index+10)%10],sequence:1842-index,delivery:index===0?'queued':'sent'}))}}
function json(response,status,payload){response.writeHead(status,{'content-type':'application/json; charset=utf-8','cache-control':'no-store'});response.end(JSON.stringify(payload))}
async function bodyParams(request){const chunks=[];for await(const chunk of request)chunks.push(chunk);return new URLSearchParams(Buffer.concat(chunks).toString())}
function statusPayload(state){const drift=Math.floor((Date.now()/1700)%5)-2;const raw=[585,1290,2048,3235].map(value=>value+drift);return {firmware_version:'1.0.0-simulator',boot_id:'SIM-7FA31C92',uptime_seconds:7324,free_heap:286112,reset_reason:'software',device_time:new Date().toISOString(),display_on:state.displayOn,scan_rate_hz:49,network:{connected:true,ssid:state.settings.wifi_ssid,ip:state.settings.static_ip,rssi:-48,ap_active:false,ap_ip:''},integration:{configured:Boolean(state.settings.backend_url),last_ok:Boolean(state.settings.backend_url),last_code:state.settings.backend_url?202:0,sequence:state.sequence},storage:{ready:true,total_bytes:127865454592,used_bytes:183742464},power:{external:true,source_label:'Rete elettrica'},snapshot:{valid:true,stable:true,weight_kg:12340,stable_for_ms:4280,digits:[1,2,3,4]},sensors:raw.map((value,index)=>({present:true,healthy:true,raw:value,status:32,agc:74+index,magnitude:1680+index*82,position:index+1}))}}

export function createSimulator(){
  const state=initialState();
  const server=createHttpServer(async(request,response)=>{
    const url=new URL(request.url,'http://localhost');
    if(request.method==='GET'&&url.pathname==='/api/status')return json(response,200,statusPayload(state));
    if(request.method==='GET'&&url.pathname==='/api/settings')return json(response,200,state.settings);
    if(request.method==='GET'&&url.pathname==='/api/calibration')return json(response,200,state.calibration);
    if(request.method==='GET'&&url.pathname==='/api/history')return json(response,200,{items:state.history.slice(0,Number(url.searchParams.get('limit')||100))});
    if(request.method==='GET'&&url.pathname==='/api/history/export'){response.writeHead(200,{'content-type':'application/x-ndjson','content-disposition':'attachment; filename=laveggio-history.ndjson'});return response.end(state.history.map(item=>JSON.stringify(item)).join('\n'))}
    if(request.method==='GET'&&url.pathname==='/api/logs'){response.writeHead(200,{'content-type':'text/plain; charset=utf-8'});return response.end('[info] device_started firmware=1.0.0-simulator\n[info] wifi_connected 192.168.0.42\n[info] sd_mounted 119.1 GB\n[info] sensor_bus_ready mux=0x70')}
    if(request.method==='GET'&&url.pathname==='/api/logs/export'){response.writeHead(200,{'content-type':'application/x-ndjson','content-disposition':'attachment; filename=laveggio-log-completo.ndjson'});return response.end(JSON.stringify({captured_at:new Date().toISOString(),event:'sensor_diagnostics',scan_rate_hz:49,sensors:statusPayload(state).sensors})+'\n')}
    if(request.method==='GET'&&url.pathname==='/api/wifi/scan')return json(response,200,{scanning:false,networks:[{ssid:'Distilleria',rssi:-48,secure:true},{ssid:'Cantina-Ospiti',rssi:-67,secure:true},{ssid:'Magazzino',rssi:-74,secure:true}]});
    if(request.method==='POST'&&url.pathname==='/api/display'){const form=await bodyParams(request);state.displayOn=form.get('enabled')==='true';return json(response,200,{ok:true})}
    if(request.method==='POST'&&url.pathname==='/api/calibration/capture'){const form=await bodyParams(request);const channel=Number(form.get('channel'));const position=Number(form.get('position'));if(!Number.isInteger(channel)||channel<0||channel>3||!Number.isInteger(position)||position<0||position>9)return json(response,400,{error:'Canale o posizione non validi'});const point=state.calibration.channels[channel].points[position];point.enabled=true;point.raw=statusPayload(state).sensors[channel].raw;return json(response,200,{ok:true,raw:point.raw})}
    if(request.method==='POST'&&url.pathname==='/api/calibration/settings'){const form=await bodyParams(request);const channel=Number(form.get('channel'));const target=state.calibration.channels[channel];if(!target)return json(response,400,{error:'Canale non valido'});target.multiplier_kg=Number(form.get('multiplier'));target.tolerance=Number(form.get('tolerance'));target.hysteresis=Number(form.get('hysteresis'));return json(response,200,{ok:true})}
    if(request.method==='POST'&&url.pathname==='/api/calibration/reset'){const form=await bodyParams(request);const target=state.calibration.channels[Number(form.get('channel'))];if(!target)return json(response,400,{error:'Canale non valido'});target.points.forEach(point=>{point.enabled=false;point.raw=0});return json(response,200,{ok:true})}
    if(request.method==='POST'&&url.pathname.startsWith('/api/settings/')){const form=await bodyParams(request);for(const [key,value] of form){if((key==='wifi_password'||key==='backend_token'||key==='admin_password')&&!value)continue;if(['use_dhcp','display_default_on','power_sense_enabled'].includes(key))state.settings[key]=value==='true';else if(['stable_ms','heartbeat_seconds'].includes(key))state.settings[key]=Number(value);else state.settings[key]=value}return json(response,200,{ok:true})}
    if(request.method==='POST'&&['/api/restart','/api/ota'].includes(url.pathname))return json(response,200,{ok:true,restart_required:true});
    if(url.pathname.startsWith('/api/'))return json(response,404,{error:'Endpoint non trovato'});
    const requested=url.pathname==='/'?'index.html':url.pathname.slice(1);const target=normalize(join(dataRoot,requested));if(!target.startsWith(dataRoot))return json(response,403,{error:'Percorso non valido'});try{const content=await readFile(target);const types={'.html':'text/html; charset=utf-8','.css':'text/css; charset=utf-8','.js':'application/javascript; charset=utf-8','.png':'image/png'};response.writeHead(200,{'content-type':types[extname(target)]||'application/octet-stream'});response.end(content)}catch{json(response,404,{error:'File non trovato'})}
  });
  return {server,state};
}

if(process.argv[1]===fileURLToPath(import.meta.url)){
  const port=Number(process.env.PORT||4177);const {server}=createSimulator();server.listen(port,'127.0.0.1',()=>console.log(`Laveggio simulator ready at http://127.0.0.1:${port}`));
}
