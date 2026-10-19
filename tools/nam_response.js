/* Frequency response of a NAM A2 Lite capture, measured in the browser.
 *
 * forward(): the capture's network, sample by sample, in the layout
 * tools/nam_prototype/generate.py writes as C (validated against NAM Core):
 * rechannel 1->3, 23 dilated causal convolutions (3->3) with the input mixed
 * in, LeakyReLU 0.01, a 1x1 residual per layer, the post-activations summed
 * into a 16-tap head, a head scale.
 *
 * measure(): an amp is not a filter, so its "response" depends on how hard it
 * is played. Pink noise (guitar-like) goes in at three RMS levels; for each,
 * H = Sxy/Sxx (Welch, Hann, 4096 points) is the part of the output that
 * follows the input: the capture's EQ curve at that level. Smoothed to 1/6
 * octave on a log grid, 40 Hz - 18 kHz, at the pedal's 44.1 kHz.
 * No uploaded code runs: the model file is numbers only. */
(function(root){
'use strict';
const D=[1,3,7,17,41,101,239,1,3,7,17,41,101,239,1,13,1,3,7,17,41,101,239];
const K=[...Array(14).fill(6),15,15,...Array(7).fill(6)];
const FS=44100,NFFT=4096,LEVELS=[{name:'Soft',db:-30},{name:'Medium',db:-18},{name:'Hard',db:-6}];

// The capture as a function: returns step(x) -> y, one sample at a time.
function forward(weights){
 let at=0;const take=n=>{const v=weights.slice(at,at+n);at+=n;return v;};
 const pre=take(3),layers=K.map((k,li)=>({k,d:D[li],w:take(9*k),b:take(3),mix:take(3),rw:take(9),rb:take(3)}));
 const head=take(48),bias=take(1)[0],scale=take(1)[0];
 if(at!==weights.length)throw Error('Unexpected weight count.');
 for(const L of layers){L.n=1<<Math.ceil(Math.log2((L.k-1)*L.d+1));L.h=new Float64Array(3*L.n);}
 const hh=new Float64Array(48);let pos=0;
 return function step(x){
  let a=x*pre[0],b=x*pre[1],c=x*pre[2],h0=0,h1=0,h2=0;
  for(const L of layers){
   const n=L.n,p=pos&(n-1),h=L.h,w=L.w,k=L.k;h[3*p]=a;h[3*p+1]=b;h[3*p+2]=c;
   let z0=L.b[0],z1=L.b[1],z2=L.b[2];
   for(let t=0;t<k;t++){const q=(p-(k-1-t)*L.d+n*8)&(n-1),qa=h[3*q],qb=h[3*q+1],qc=h[3*q+2];
    // NAM stores convolution weights [out][in][tap]
    z0+=w[(0*3+0)*k+t]*qa+w[(0*3+1)*k+t]*qb+w[(0*3+2)*k+t]*qc;
    z1+=w[(1*3+0)*k+t]*qa+w[(1*3+1)*k+t]*qb+w[(1*3+2)*k+t]*qc;
    z2+=w[(2*3+0)*k+t]*qa+w[(2*3+1)*k+t]*qb+w[(2*3+2)*k+t]*qc;}
   z0+=L.mix[0]*x;z1+=L.mix[1]*x;z2+=L.mix[2]*x;
   if(z0<0)z0*=0.01;if(z1<0)z1*=0.01;if(z2<0)z2*=0.01;
   h0+=z0;h1+=z1;h2+=z2;
   const r=L.rw,rb=L.rb;a+=rb[0]+r[0]*z0+r[1]*z1+r[2]*z2;b+=rb[1]+r[3]*z0+r[4]*z1+r[5]*z2;c+=rb[2]+r[6]*z0+r[7]*z1+r[8]*z2;
  }
  const p=pos&15;hh[3*p]=h0;hh[3*p+1]=h1;hh[3*p+2]=h2;
  let y=bias;for(let t=0;t<16;t++){const q=(p-(15-t)+16)&15;y+=head[0*16+t]*hh[3*q]+head[1*16+t]*hh[3*q+1]+head[2*16+t]*hh[3*q+2];}
  pos++;return y*scale;
 };
}

function fft(re,im){                           // in place, radix 2, forward
 const n=re.length;for(let i=1,j=0;i<n;i++){let bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){[re[i],re[j]]=[re[j],re[i]];[im[i],im[j]]=[im[j],im[i]];}}
 for(let len=2;len<=n;len<<=1){const a=-2*Math.PI/len,wr=Math.cos(a),wi=Math.sin(a);
  for(let i=0;i<n;i+=len){let cr=1,ci=0;for(let j=0;j<len/2;j++){const u=i+j,v=u+len/2,tr=re[v]*cr-im[v]*ci,ti=re[v]*ci+im[v]*cr;
   re[v]=re[u]-tr;im[v]=im[u]-ti;re[u]+=tr;im[u]+=ti;const t=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=t;}}}
}

function pink(n,seed){                         // Paul Kellet's filter over a seeded PRNG, RMS 1
 let s=seed>>>0;const rnd=()=>{s=(s+0x6D2B79F5)>>>0;let t=s;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return((t^t>>>14)>>>0)/4294967296*2-1;};
 const out=new Float64Array(n);let b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0,ss=0;
 for(let i=0;i<n;i++){const w=rnd();b0=.99886*b0+w*.0555179;b1=.99332*b1+w*.0750759;b2=.969*b2+w*.153852;b3=.8665*b3+w*.3104856;b4=.55*b4+w*.5329522;b5=-.7616*b5-w*.016898;
  out[i]=b0+b1+b2+b3+b4+b5+b6+w*.5362;b6=w*.115926;ss+=out[i]*out[i];}
 const g=1/Math.sqrt(ss/n);for(let i=0;i<n;i++)out[i]*=g;return out;
}

// H = Sxy/Sxx in dB per FFT bin (Welch, 50% overlap), and the gain y/x in dB.
function transfer(x,y){
 const win=Float64Array.from({length:NFFT},(_,i)=>0.5-0.5*Math.cos(2*Math.PI*i/NFFT)),half=NFFT/2;
 const sxx=new Float64Array(half+1),sxr=new Float64Array(half+1),sxi=new Float64Array(half+1);
 const xr=new Float64Array(NFFT),xi=new Float64Array(NFFT),yr=new Float64Array(NFFT),yi=new Float64Array(NFFT);
 for(let s=0;s+NFFT<=x.length;s+=half){
  for(let i=0;i<NFFT;i++){xr[i]=x[s+i]*win[i];yr[i]=y[s+i]*win[i];xi[i]=yi[i]=0;}fft(xr,xi);fft(yr,yi);
  for(let k=0;k<=half;k++){sxx[k]+=xr[k]*xr[k]+xi[k]*xi[k];sxr[k]+=yr[k]*xr[k]+yi[k]*xi[k];sxi[k]+=yi[k]*xr[k]-yr[k]*xi[k];}
 }
 return Float64Array.from(sxx,(v,k)=>10*Math.log10((sxr[k]*sxr[k]+sxi[k]*sxi[k])/(v*v)+1e-30));
}

const grid=()=>Array.from({length:200},(_,i)=>40*Math.pow(18000/40,i/199));
// dB per bin -> 1/6-octave power average on the log grid; bins are fs/NFFT apart.
// Where a band covers fewer than two bins (the bass), interpolate between bins.
function smooth(db,fs,freqs){
 return freqs.map(f=>{const lo=f*Math.pow(2,-1/12),hi=f*Math.pow(2,1/12),kc=f*NFFT/fs;let s=0,n=0;
  if((hi-lo)*NFFT/fs<2){const k=Math.floor(kc),t=kc-k;return k+1<db.length?db[k]*(1-t)+db[k+1]*t:db[db.length-1];}
  for(let k=Math.max(1,Math.floor(lo*NFFT/fs));k<=Math.min(db.length-1,Math.ceil(hi*NFFT/fs));k++){s+=Math.pow(10,db[k]/10);n++;}
  return n?10*Math.log10(s/n):NaN;});
}

// Each level's curve, plus its gain: runs the capture 3 x 2^15 samples (a second or two).
async function measure(weights,{onProgress,yieldEvery=4096}={}){
 const n=1<<15,warm=4096,freqs=grid(),curves=[];
 for(const [li,L] of LEVELS.entries()){
  const step=forward(weights),amp=Math.pow(10,L.db/20),x=pink(n+warm,1234+li),y=new Float64Array(n+warm);
  for(let i=0;i<n+warm;i++){x[i]*=amp;y[i]=step(x[i]);if(i%yieldEvery===0&&yieldEvery){if(onProgress)onProgress((li+i/(n+warm))/LEVELS.length);await new Promise(r=>setTimeout(r));}}
  const xs=x.subarray(warm),ys=y.subarray(warm);let px=0,py=0;for(let i=0;i<n;i++){px+=xs[i]*xs[i];py+=ys[i]*ys[i];}
  curves.push({name:L.name,inputDb:L.db,gainDb:10*Math.log10(py/px),db:smooth(transfer(xs,ys),FS,freqs)});
 }
 return {freqs,curves};
}

const api={forward,fft,pink,transfer,smooth,grid,measure,LEVELS,FS,NFFT};
if(typeof module!=='undefined')module.exports=api;else root.NAMResponse=api;
})(globalThis);
