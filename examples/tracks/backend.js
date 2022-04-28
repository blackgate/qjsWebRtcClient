import { setTimeout } from "os";
import { 
    RTCPeerConnection, 
    RTC_CODEC_H264,
    RTC_DIRECTION_SENDONLY, 
    RTC_NAL_SEPARATOR_LENGTH 
} from "../../build/libqjsWebRtcClient.so";

function waitFor(ms) {
    return new Promise(resolve => {
        setTimeout(resolve, ms);
    });
} 

const peerConn = new RTCPeerConnection({
    'iceServers': [
        {'urls': ['stun:stun.l.google.com:19302']}
    ]
});

console.log("Creating ...");

peerConn.onlocaldescription = d => {
    console.log("Get joiners to call: ", "join(",
        JSON.stringify(d),
    ")");
}

const dataChannel = peerConn.createDataChannel('test');
const track1 = peerConn.addTrack({
    "cname": "video1",
    "codec": RTC_CODEC_H264,
    "direction": RTC_DIRECTION_SENDONLY,
    "msid": "stream1",
    "ssrc": 1234,
    "payloadType": 102,
    "nalUnitSeparator": RTC_NAL_SEPARATOR_LENGTH
});

function syncPlaybackTime(elapsedTime) {
    const elapsedTimestamp = track1.secondsToTimestamp(elapsedTime);
    track1.currentTimestamp = track1.startTimestamp + elapsedTimestamp;
    const elapsedReportTimestamp = track1.currentTimestamp - track1.previousReportedTimestamp;
    if (track1.timestampToSeconds(elapsedReportTimestamp) > 1) {
        track1.setNeedsToReport();
    }
}

track1.onopen = async () => {
    let startTime = Date.now();
    let fps = 30;
    track1.setStartTime(startTime / 1000);
    track1.startRecording();
    for (let i = 0 ; i < 900 ; i++) {
        const expected = (i * 1000) / fps;
        const actual = Date.now() - startTime;
        if (expected > actual) await waitFor(expected - actual);
        syncPlaybackTime(expected / 1000);
        let sample = `h264/sample-${i}.h264`;
        let f = std.open(sample, "r");
        let bufLen = 1024 * 1024; // 1MB
        let buf = new ArrayBuffer(bufLen);
        let bytesRead = 0;
        while ((bytesRead = f.read(buf, 0, bufLen)) > 0) {
            track1.send(bytesRead === bufLen ? buf : buf.slice(0, bytesRead));
        }
        f.close(f);
    }
}

dataChannel.onopen = (e) => {
    globalThis.say = (msg) => { dataChannel.send(msg); };
    console.log('Say things with say("hi")');
};

dataChannel.onmessage = (data) => { console.log('Got message:', data); };

peerConn.createOffer();

globalThis.gotAnswer = (answer) => {
    console.log("Initializing ...");
    peerConn.setRemoteDescription(answer);
};

// prevent objects from being gc
globalThis.dataChannel = dataChannel;
globalThis.track1 = track1;
globalThis.peerConn = peerConn;