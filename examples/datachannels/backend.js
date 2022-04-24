import { RTCPeerConnection } from "../../build/libqjsWebRtcClient.so";

const peerConn = new RTCPeerConnection({
    'iceServers': [
        {'urls': ['stun:stun.l.google.com:19302']}
    ]
});

console.log('Call create(), or join("some offer")');

globalThis.create = function create() {
    console.log("Creating ...");

    peerConn.onlocaldescription = d => {
        console.log("Get joiners to call: ", "join(",
            JSON.stringify(d),
        ")");
    }

    const dataChannel = peerConn.createDataChannel('test');

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

    globalThis.dataChannel = dataChannel;
}

globalThis.join = function join(offer) {
    console.log("Joining ...");

    peerConn.ondatachannel = (dataChannel) => {
        globalThis.dataChannel = dataChannel;

        dataChannel.onopen = (e) => {
            globalThis.say = (msg) => { dataChannel.send(msg); };
            console.log('Say things with say("hi")');
        };
        
        dataChannel.onmessage = (data) => { console.log('Got message:', data); }
    };

    peerConn.onicegatheringstatechange = s => {
        if (s == "complete") {
            console.log("Get the creator to call: gotAnswer(",
                JSON.stringify(peerConn.localDescription),
            ")");
        }
    }

    peerConn.setRemoteDescription(offer);
    peerConn.createAnswer();
}

globalThis.peerConn = peerConn;