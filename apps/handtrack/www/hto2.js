// hto2.js - handtrack to o2
//
// Roger B. Dannenberg
// Jun 2024
//
// based on code from https://victordibia.com/handtrack.js/#/

//--------------------- Handtrack Interface -----------------

const video = document.getElementById("video");
const canvas = document.getElementById("canvas");
const context = canvas.getContext("2d");
let trackButton = document.getElementById("trackbutton");
let updateNote = document.getElementById("updatenote");

let isVideo = false;
let model = null;

const modelParams = {
    flipHorizontal: true,   // flip e.g for video  
    maxNumBoxes: 20,        // maximum number of boxes to detect
    iouThreshold: 0.5,      // ioU threshold for non-max suppression
    scoreThreshold: 0.6,    // confidence threshold for predictions.
}


function startVideo() {
    handTrack.startVideo(video).then(function (status) {
        console.log("video started", status);
        if (status) {
            updateNote.innerText = "Video started. Now tracking";
            isVideo = true;
            runDetection();
        } else {
            updateNote.innerText = "Please enable video";
        }
    });
}


function stopVideo() {
    updateNote.innerText = "Stopping video";
    handTrack.stopVideo(video);
    isVideo = false;
    updateNote.innerText = "Video stopped";
}


function toggleVideo() {
    if (!isVideo) {
        updateNote.innerText = "Starting video";
        startVideo();
    } else {
        stopVideo();
    }
}


function print_predictions(predictions) {
    for (prediction of predictions) {
        console.log(prediction.label, prediction.class, prediction.bbox[0]);
    }
}


function runDetection() {
    model.detect(video).then(predictions => {
        // console.log("Predictions: ", predictions);
        print_predictions(predictions);
        send_predictions(predictions);
        video.style.height = 240;
        model.renderPredictions(predictions, canvas, context, video);
        if (isVideo) {
            requestAnimationFrame(runDetection);
        }
    });
}


//-------------------- O2lite Interface -----------------

function o2ws_on_error(msg) {
    console.log(msg);
}


function o2ws_status_msg(msg) {
    updateNote.innerText = msg;
}


function hto2_initialize(ensemble) {
    o2ws_initialize(ensemble);
    o2ws_status_msgs_enable = true;
    // Load the model.
    handTrack.load(modelParams).then(lmodel => {
        // detect objects in the image.
        model = lmodel
        updateNote.innerText = "Loaded Model!"
        trackButton.disabled = false
    });
}


function send_predictions(predictions) {
    for (p of predictions) {
        o2ws_send("/htclient/obj", 0, "ifffff", p.class, parseFloat(p.score),
                  p.bbox[0], p.bbox[1], p.bbox[2], p.bbox[3]);
    }
}
