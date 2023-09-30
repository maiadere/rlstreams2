var file = File.openDialog("Select file");

function parseFrame(frame) {
    var frameData = frame.split(';').map(function (x) {
        x = x.split(',').map(parseFloat);
        var v = ({
            "location": {
                "x": x[0],
                "y": x[1],
                "z": x[2],
            },
            "rotation": {
                "x": x[3],
                "y": x[4],
                "z": x[5],
                "w": x[6],
            },
        });
        if (x.length == 8) {
            v['fov'] = x[7];
        }
        return v;
    });
    var cars = frameData.slice(2);
    cars.pop();
    return ({
        "camera": frameData[0],
        "ball": frameData[1],
        "cars": cars,
    });
}

if (file && file.open("r")) {
    var data = file.read().split("|");
    data.pop();
    data = data.map(parseFrame);
    file.close();
}

alert(JSON.stringify(data[0].cars.length).substring(0, 200));

app.beginUndoGroup("Camera and Null");
var myComp = app.project.activeItem;

var w = myComp.width / 2;
var h = myComp.height / 2;
var camera = myComp.layers.addCamera("Camera", [w, h]);
camera.autoOrient = AutoOrientType.NO_AUTO_ORIENT;

var ballLayer = myComp.layers.addNull();
ballLayer.name = "Ball";
ballLayer.threeDLayer = true;

var cars = [];

if (data[0].cars) {
    for (var i = 0; i < data[0].cars.length; i++) {
        var car = myComp.layers.addNull();
        car.name = 'Car ' + (i + 1);
        car.threeDLayer = true;
        cars.push(car);
    }
}

var frameDuration = app.project.activeItem.frameDuration;

function sign(x) { return ((x > 0) - (x < 0)) || +x; };

function GetRotation(r) {
    var qX = r.x;
    var qY = r.y;
    var qZ = r.z;
    var qW = r.w;
    var H1 = (2 * qY * qW) - (2 * qX * qZ);
    var H2 = 1 - (2 * qY * qY) - (2 * qZ * qZ);
    var Pitch = Math.atan2(H1, H2);
    var A = 2 * (qX * qY + qZ * qW);
    var Yaw = Math.abs(A) >= 1 ? (sign(A) * Math.PI / 2) : Math.asin(A);
    var B1 = (2 * qX * qW) - (2 * qY * qZ);
    var B2 = 1 - (2 * qX * qX) - (2 * qZ * qZ);
    var Roll = Math.atan2(B1, B2);
    var RadToDeg = 180 / Math.PI;
    var NewPitch = Pitch * RadToDeg;
    var NewYaw = Yaw * RadToDeg;
    var NewRoll = Roll * RadToDeg;
    var Rotation = new Object();
    Rotation.X = NewPitch * -1;
    Rotation.Y = NewYaw;
    Rotation.Z = NewRoll * -1;
    return Rotation;
}

var timesArray = [];
var cameraZooms = [];
var cameraPositions = [];
var cameraRotationsX = [];
var cameraRotationsY = [];
var cameraRotationsZ = [];

var ballPositions = [];
var ballRotationsX = [];
var ballRotationsY = [];
var ballRotationsZ = [];

var carsPositionsArrays = [];
var carsRotationsXArrays = [];
var carsRotationsYArrays = [];
var carsRotationsZArrays = [];

for (var i = 0; i < cars.length; i++) {
    carsPositionsArrays.push([]);
    carsRotationsXArrays.push([]);
    carsRotationsYArrays.push([]);
    carsRotationsZArrays.push([]);
}

for (var i = 0; i < data.length; i++) {
    // Camera
    var frame = data[i].camera;
    var rotation = GetRotation(frame.rotation);
    timesArray.push(i * frameDuration);
    cameraZooms.push((myComp.width / 2.0) / Math.tan((frame.fov * 3.1415926 / 180.0) / 2.0));
    cameraPositions.push([frame.location.y * 2.54, frame.location.z * -2.54, frame.location.x * 2.54]);
    cameraRotationsX.push(rotation.X);
    cameraRotationsY.push(rotation.Y);
    cameraRotationsZ.push(rotation.Z);
    $.gc();

    // Ball
    var ball = data[i].ball;
    var rotation = GetRotation(ball.rotation);
    ballPositions.push([ball.location.y * 2.54, ball.location.z * -2.54, ball.location.x * 2.54]);
    ballRotationsX.push(rotation.X);
    ballRotationsY.push(rotation.Y);
    ballRotationsZ.push(rotation.Z);
    $.gc();

    // Cars
    for (var j = 0; j < cars.length; j++) {
        var car = data[i].cars[j];
        var rotation = GetRotation(car.rotation);
        carsPositionsArrays[j].push([car.location.y * 2.54, car.location.z * -2.54, car.location.x * 2.54]);
        carsRotationsXArrays[j].push(rotation.X);
        carsRotationsYArrays[j].push(rotation.Y);
        carsRotationsZArrays[j].push(rotation.Z);
        $.gc();
    }
}

camera.property("Zoom").setValuesAtTimes(timesArray, cameraZooms);
camera.property("Position").setValuesAtTimes(timesArray, cameraPositions);
camera.property("Rotation X").setValuesAtTimes(timesArray, cameraRotationsX);
camera.property("Rotation Y").setValuesAtTimes(timesArray, cameraRotationsY);
camera.property("Rotation Z").setValuesAtTimes(timesArray, cameraRotationsZ);

ballLayer.property("Position").setValuesAtTimes(timesArray, ballPositions);
ballLayer.property("Rotation X").setValuesAtTimes(timesArray, ballRotationsX);
ballLayer.property("Rotation Y").setValuesAtTimes(timesArray, ballRotationsY);
ballLayer.property("Rotation Z").setValuesAtTimes(timesArray, ballRotationsZ);

for (var j = 0; j < cars.length; j++) {
    var car = cars[j];
    car.property("Position").setValuesAtTimes(timesArray, carsPositionsArrays[j]);
    car.property("Rotation X").setValuesAtTimes(timesArray, carsRotationsXArrays[j]);
    car.property("Rotation Y").setValuesAtTimes(timesArray, carsRotationsYArrays[j]);
    car.property("Rotation Z").setValuesAtTimes(timesArray, carsRotationsZArrays[j]);
}
