#define saturate(a) clamp(a, 0.0, 1.0)

const float kKeyLeft  = 37.5 / 256.0;
const float kKeyUp    = 38.5 / 256.0;
const float kKeyRight = 39.5 / 256.0;
const float kKeyDown  = 40.5 / 256.0;
const float kKeySpace = 32.5 / 256.0;

float SampleKey(float key)
{
	return step(0.5, texture(iChannel1, vec2(key, 0.25)).x);
}

float localTimeDelta = 0.0;

// -------------------- Start of shared state / variables --------------------
#define PI 3.141592653
const ivec2 kTexState1 = ivec2(0, 160);
const float gameGridRadius = 16.0;
const vec2  gameGridCenter = vec2(80.0, 80.0);
const vec2 iotower = vec2(100.5, 6.5);
// Thumbnail is 288x162 res - actually 256x144 is smallest. :(
const vec2 smallestResolution = vec2(288.0, 162.0);

vec3 bikeAPos = vec3(0.0);
float bikeAAngle = 0.0;
vec3 bikeBPos = vec3(0.0);
float bikeBAngle = 0.0;
vec4 keyStateLURD = vec4(0.0);
vec2 rotQueueA = vec2(0.0);
vec2 rotQueueB = vec2(0.0);
float bitsCollected = 0.0;
float framerate = 0.0;
float gameStage = -1.0;
float message = 0.0;
vec4 camFollowPos = vec4(0.0);
vec4 indicatorPulse = vec4(0.0);
vec4 animA = vec4(0);

vec4 LoadValue(ivec2 uv) { return texelFetch(iChannel0, uv, 0); }

void StoreValue(ivec2 st, vec4 value, inout vec4 fragColor, in vec2 fragCoord)
{ fragColor = (ivec2(fragCoord.xy) == st)? value : fragColor; }

void LoadState()
{
    vec4 state1 = LoadValue(kTexState1);
    bikeAPos = state1.xyz;
    bikeAAngle = state1.w;
    keyStateLURD = LoadValue(kTexState1 + ivec2(1, 0));
    vec4 tempRotQueue = LoadValue(kTexState1 + ivec2(2, 0));
    rotQueueA = tempRotQueue.xy;
    rotQueueB = tempRotQueue.zw;
    vec4 state2 = LoadValue(kTexState1 + ivec2(3, 0));
    bitsCollected = state2.x;
    framerate = state2.y;
    gameStage = state2.z;
    message = state2.w;
    camFollowPos = LoadValue(kTexState1 + ivec2(4, 0));
    indicatorPulse = LoadValue(kTexState1 + ivec2(5, 0));
    vec4 state3 = LoadValue(kTexState1 + ivec2(6, 0));
    bikeBPos = state3.xyz;
    bikeBAngle = state3.w;
    animA = LoadValue(kTexState1 + ivec2(7, 0));
}

void StoreState(inout vec4 fragColor, in vec2 fragCoord)
{
    vec4 state1 = vec4(bikeAPos, bikeAAngle);
    StoreValue(kTexState1, state1, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(1, 0), keyStateLURD, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(2, 0), vec4(rotQueueA.xy, rotQueueB), fragColor, fragCoord);
    vec4 state2 = vec4(bitsCollected, framerate, gameStage, message);
    StoreValue(kTexState1 + ivec2(3, 0), state2, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(4, 0), camFollowPos, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(5, 0), indicatorPulse, fragColor, fragCoord);
    vec4 state3 = vec4(bikeBPos, bikeBAngle);
    StoreValue(kTexState1 + ivec2(6, 0), state3, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(7, 0), animA, fragColor, fragCoord);
}
// -------------------- End of shared state / variables --------------------

// Font rendering macros
#define _SPACE if (uv == cursorPos) letter = 32.0; cursorPos.x++;
#define _EXCLAMATION if (uv == cursorPos) letter = 33.0; cursorPos.x++;
#define _SLASH if (uv == cursorPos) letter = 47.0; cursorPos.x++;
#define _A if (uv == cursorPos) letter = 65.0; cursorPos.x++;
#define _B if (uv == cursorPos) letter = 66.0; cursorPos.x++;
#define _C if (uv == cursorPos) letter = 67.0; cursorPos.x++;
#define _D if (uv == cursorPos) letter = 68.0; cursorPos.x++;
#define _E if (uv == cursorPos) letter = 69.0; cursorPos.x++;
#define _F if (uv == cursorPos) letter = 70.0; cursorPos.x++;
#define _G if (uv == cursorPos) letter = 71.0; cursorPos.x++;
#define _H if (uv == cursorPos) letter = 72.0; cursorPos.x++;
#define _I if (uv == cursorPos) letter = 73.0; cursorPos.x++;
#define _J if (uv == cursorPos) letter = 74.0; cursorPos.x++;
#define _K if (uv == cursorPos) letter = 75.0; cursorPos.x++;
#define _L if (uv == cursorPos) letter = 76.0; cursorPos.x++;
#define _M if (uv == cursorPos) letter = 77.0; cursorPos.x++;
#define _N if (uv == cursorPos) letter = 78.0; cursorPos.x++;
#define _O if (uv == cursorPos) letter = 79.0; cursorPos.x++;
#define _P if (uv == cursorPos) letter = 80.0; cursorPos.x++;
#define _Q if (uv == cursorPos) letter = 81.0; cursorPos.x++;
#define _R if (uv == cursorPos) letter = 82.0; cursorPos.x++;
#define _S if (uv == cursorPos) letter = 83.0; cursorPos.x++;
#define _T if (uv == cursorPos) letter = 84.0; cursorPos.x++;
#define _U if (uv == cursorPos) letter = 85.0; cursorPos.x++;
#define _V if (uv == cursorPos) letter = 86.0; cursorPos.x++;
#define _W if (uv == cursorPos) letter = 87.0; cursorPos.x++;
#define _X if (uv == cursorPos) letter = 88.0; cursorPos.x++;
#define _Y if (uv == cursorPos) letter = 89.0; cursorPos.x++;
#define _Z if (uv == cursorPos) letter = 90.0; cursorPos.x++;
#define _0 if (uv == cursorPos) letter = 48.0; cursorPos.x++;
#define _1 if (uv == cursorPos) letter = 49.0; cursorPos.x++;
#define _2 if (uv == cursorPos) letter = 50.0; cursorPos.x++;
#define _3 if (uv == cursorPos) letter = 51.0; cursorPos.x++;
#define _4 if (uv == cursorPos) letter = 52.0; cursorPos.x++;
#define _5 if (uv == cursorPos) letter = 53.0; cursorPos.x++;
#define _6 if (uv == cursorPos) letter = 54.0; cursorPos.x++;
#define _7 if (uv == cursorPos) letter = 55.0; cursorPos.x++;
#define _8 if (uv == cursorPos) letter = 56.0; cursorPos.x++;
#define _9 if (uv == cursorPos) letter = 57.0; cursorPos.x++;

void ResetAll(vec2 fragCoord, inout vec4 finalOut)
{
    bikeAPos = vec3(0.5, 1.0, 0.5);
    bikeAAngle = 0.0;
    bikeBPos = vec3(0.5, 1.0, 2.5);
    bikeBAngle = 0.0;
    bitsCollected = 0.0;
	camFollowPos = vec4(sin(iTime*0.5)*6.0, 2.5, cos(iTime*0.5)*6.0, 0.0);
    gameStage = -1.0;
    message = 81.0;
    animA = vec4(256.0, 0, 0, 0);
    indicatorPulse = vec4(0, -256.0, 0, 0);

    float tex = texelFetch(iChannel2, ivec2(fragCoord.xy), 0).x;
    finalOut = vec4(0);
    float inGameGrid = max(abs(fragCoord.x - gameGridCenter.x), abs(fragCoord.y - gameGridCenter.y));
    if (inGameGrid < gameGridRadius) {
        finalOut = vec4(0.0);
        // Border around gamegrid
        if (inGameGrid >= gameGridRadius - 1.0) finalOut.x = 2.0;
    } else {
        finalOut.x = tex*20.0-8.0;
        finalOut.x = max(-1.0, finalOut.x);

        // Make maze
	  	vec2 uv = fragCoord.xy;///iResolution.xy;
        uv = uv / 12.0;
        vec2 uvf = fract(uv);
        float rnd = texelFetch(iChannel3, ivec2(uv), 0).x;
        float hv = ((rnd>.5)?uvf.x:uvf.y);
        //finalOut.yz = uvf*0.0001;//smoothstep(.5,.6, hv)*80.0);
        finalOut.x *= smoothstep(.5,.55, hv);// + uvf.x*uvf.y;
		//finalOut.x = (hv/256.0);

        // I/O tower base
        float tower = distance(fragCoord - gameGridCenter, iotower);
        if (tower <= 17.5) {
            finalOut.xy = vec2(max(0.0, 9.5 - round(tower)), 0.0);
            if (fragCoord.y - gameGridCenter.y == iotower.y) finalOut.xy = vec2(0.0, 0.0);
        }

        // Place power-ups
        if (distance(fragCoord, vec2(80.0+20.5, 50.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0-40.5, 50.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0-28.5, 125.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+42.5, 122.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+67.5, 27.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+55.5, 75.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+78.5, 86.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+42.5, 82.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+96.5, 122.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+128.5, 63.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0-4.5, 57.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0-4.5, 102.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);
        if (distance(fragCoord, vec2(80.0+54.5, 52.5)) <= 0.5) finalOut.xy = vec2(1.0, 256.0);

        if (inGameGrid <= gameGridRadius + 5.0) finalOut.x = 0.0;
        finalOut.xy = floor(finalOut.xy);
    }
}

float TextRenderer(inout vec4 fragColor, in vec2 fragCoord) {
    // Font printing and HUD
    float letter = 32.0;
    ivec2 uv = ivec2(fragCoord);
    if ((bitsCollected > 0.0) && (uv == ivec2(0, 0))) letter = 48.0 + bitsCollected;

    // Frame rate printouts
    //if (uv == ivec2(0, 1)) letter = 48.0 + floor(framerate);
    //if (uv == ivec2(1, 1)) letter = 48.0 + mod(floor(framerate*10.0),10.0);
    //if (uv == ivec2(2, 1)) letter = 48.0 + mod(floor(framerate*100.0),10.0);

    ivec2 cursorPos = ivec2(1, 2);
    if ((message >= 10.0) && (message <= 18.0)) {
        cursorPos = ivec2(3, 1);
        _E _S _C _A _P _E _SPACE _T _H _E;
        cursorPos = ivec2(4, 2);
        _G _A _M _E _G _R _I _D;
    }
    if ((message >= 20.0) && (message <= 28.0)) {
        cursorPos = ivec2(6, 1);
        _D _E _A _D;
    }
    if ((message >= 30.0) && (message <= 38.0)) {
        cursorPos = ivec2(5, 1);
        _E _S _C _A _P _E _EXCLAMATION;
    }
    /*if ((message >= 40.0) && (message <= 48.0)) {
        cursorPos = ivec2(1, 1);
        _B _U _F _F _E _R _SPACE _O _V _E _R _R _U _N;
        cursorPos = ivec2(0, 2);
        _P _R _O _G _R _A _M _S _SPACE _E _S _C _A _P _E _D;
    }*/
    if ((message >= 50.0) && (message <= 58.0)) {
        cursorPos = ivec2(5, 1);
        _G _O _SPACE _T _O;
        cursorPos = ivec2(3, 2);
        _I _SLASH _O _SPACE _T _O _W _E _R;
    }
    if ((message >= 60.0) && (message <= 68.0)) {
        cursorPos = ivec2(4, 1);
        _T _R _A _N _S _M _I _T;
    }
    if ((message >= 70.0) && (message <= 78.0)) {
        cursorPos = ivec2(1, 1);
        _C _O _L _L _E _C _T _SPACE _8 _SPACE _B _I _T _S;
    }
    if ((message >= 80.0) && (message <= 88.0)) {
        cursorPos = ivec2(1, 6);
        _C _L _I _C _K _SPACE _O _R _SPACE _S _P _A _C _E;
        cursorPos = ivec2(5, 7);
        _S _T _A _R _T _S;
    }
    if ((message >= 90.0) && (message <= 98.0)) {
        cursorPos = ivec2(2, 1);
        _Y _O _U _SPACE _D _I _D _SPACE _N _O _T;
        cursorPos = ivec2(5, 2);
        _E _S _C _A _P _E;
        cursorPos = ivec2(3, 3);
        _T _R _Y _SPACE _A _G _A _I _N;
    }
    if ((message >= 100.0) && (message <= 108.0)) {
        cursorPos = ivec2(3, 1);
        _E _S _C _A _P _E _SPACE _T _H _E;
        cursorPos = ivec2(4, 2);
        _G _A _M _E _G _R _I _D;
        cursorPos = ivec2(7, 5);
        _B _Y;
        cursorPos = ivec2(3, 6);
        _O _T _A _V _I _O _SPACE _G _O _O _D;
    }

    return letter;
}

// Print a single floating point number to the screen
void PrintNum(inout float letter, in vec2 fragCoord, float num, ivec2 pos) {
    ivec2 uv = ivec2(fragCoord);

    // Print 4 whole number digits, a decimal point, and 2 fractional digits.
    if (uv == ivec2(pos.x + 0, pos.y)) {
        letter = 48.0 + floor(mod(num/1000.0, 10.0));
        if (num < 0.0) letter = 45.0;
    }
    num = abs(num);
    if (uv == ivec2(pos.x + 1, pos.y)) letter = 48.0 + floor(mod(num/100.0, 10.0));
    else if (uv == ivec2(pos.x + 2, pos.y)) letter = 48.0 + floor(mod(num/10.0, 10.0));
    else if (uv == ivec2(pos.x + 3, pos.y)) letter = 48.0 + floor(mod(num, 10.0));
    else if (uv == ivec2(pos.x + 4, pos.y)) letter = 46.0;
    else if (uv == ivec2(pos.x + 5, pos.y)) letter = 48.0 + mod(floor(num*10.0),10.0);
    else if (uv == ivec2(pos.x + 6, pos.y)) letter = 48.0 + mod(floor(num*100.0),10.0);
}

void MoveBikeA(inout vec4 finalOut, in vec2 fragCoord) {
    if ((gameStage >= 100.0) && (gameStage < 104.0)) {
        return;
    }
    vec4 keyOrigLURD = keyStateLURD;
    keyStateLURD = vec4(SampleKey(kKeyLeft), SampleKey(kKeyUp),
                        SampleKey(kKeyRight), SampleKey(kKeyDown));

    // Handle mouse clicks for left / right - for mobile phone controls.
    float lastMouseX = animA.w;
    if ((lastMouseX < 0.0) && (iMouse.z > 0.0)) {
        if (abs(iMouse.z) < iResolution.x / 2.0) keyStateLURD.x = 1.0;
        if (abs(iMouse.z) >= iResolution.x / 2.0) keyStateLURD.z = 1.0;
    }
    animA.w = iMouse.z;

    if ((keyStateLURD.x > 0.5) && (keyStateLURD.x != keyOrigLURD.x)) {
        if (rotQueueA.x == 0.0) rotQueueA.x = -1.0;
        else rotQueueA.y = -1.0;
    }
    if ((keyStateLURD.z > 0.5) && (keyStateLURD.z != keyOrigLURD.z)) {
        if (rotQueueA.x == 0.0) rotQueueA.x = 1.0;
        else rotQueueA.y = 1.0;
    }

    // Move the bike
    vec3 oldPos = bikeAPos;
    vec3 dir = round(vec3(cos(bikeAAngle), 0.0, -sin(bikeAAngle)));
    float cycleASpeed = min(1.0, localTimeDelta*4.0);//4
    bikeAPos += dir * cycleASpeed;
    if (floor(bikeAPos+0.5) != floor(oldPos+0.5)) {
        if (rotQueueA != vec2(0.0)) {
            // Turn the bike exactly at the grid crossing
            vec3 newPos = floor(bikeAPos)+vec3(0.5, 0.0, 0.5);
            float remainder = length(bikeAPos - newPos);
            bikeAPos = newPos;
            bikeAAngle += rotQueueA.x * 3.141592653*0.5;
            // Cycle the input queue
            rotQueueA.x = 0.0;
            rotQueueA = rotQueueA.yx;
            // Add the remaining movement vector, but in the new direction
            dir = round(vec3(cos(bikeAAngle), 0.0, -sin(bikeAAngle)));
            bikeAPos += dir * remainder;
        }

        // This must happen after the turn, or we will crash into walls we turned away from
        ivec2 tpos = ivec2(floor(bikeAPos.xz+0.5*dir.xz)+gameGridCenter);
        vec4 tex = texelFetch(iChannel0, tpos, 0);
        //temp = tex.x;
        // If not a powerup
        if (tex.y < 256.0) {
            // Death by wall
            if (floor(tex.x) >= floor(bikeAPos.y)) {
                gameStage = 1000.0;
                message = 28.0;
            }
            // Death by pit
            if (floor(tex.x+1.0) < floor(bikeAPos.y)) {
                gameStage = 1000.0;
                message = 28.0;
            }
        }
    }

    ivec2 tpos = ivec2(bikeAPos.xz+gameGridCenter);
    vec4 tex = texelFetch(iChannel0, tpos, 0);

    float inGameGrid = max(abs(bikeAPos.x), abs(bikeAPos.z));
    // If the bike is in the gamegrid and the pixel in in the gamegrid part of the buffer
    if ((floor(bikeAPos.xz) == floor(fragCoord - gameGridCenter)) &&
        (inGameGrid < gameGridRadius)) {
        finalOut.x = round(bikeAPos.y);
        int trail = int(tex.y);
        if (abs(dir.x) > abs(dir.z)) {  // If driving down the x axis
            if (fract(bikeAPos.x) < 0.5) trail |= 1;
            else trail |= 2;
        } else {
            if (fract(bikeAPos.z) < 0.5) trail |= 4;
            else trail |= 8;
        }
        finalOut.y = float(trail);
    }
    // pick up power ups
    if (tex.y == 256.0) {
        bitsCollected += 1.0;
        if (ivec2(fragCoord) == tpos) {
            finalOut.xy = vec2(0.0, 0.0);
        }
        indicatorPulse = vec4(bikeAPos, 1.0);
        if (bitsCollected >= 8.0) {
            gameStage = 300.0;
            message = 55.0;
        }
    }
    camFollowPos.xyz = mix(camFollowPos.xyz, bikeAPos - dir * 5.0 + vec3(0, 2.4, 0), 0.05*localTimeDelta*60.0);

    if ((gameStage >= 100.0) && (gameStage < 200.0) && (inGameGrid > gameGridRadius)) {
        gameStage = 200.0;
        message = 75.0; // collect 8 bits
    }
    // If I/O tower is enabled and you touch it, win game.
    if (distance(bikeAPos.xz, iotower) < 0.5) {
        if ((gameStage >= 300.0) && (gameStage < 400.0)) {
            gameStage = 500.0;
            message = 63.0; // transmit
        } else if (gameStage < 300.0) {
            message = 75.0; // collect 8 bits
        }
    }
}

void MoveBikeB(inout vec4 finalOut, in vec2 fragCoord) {
    float cycleBSpeed = min(1.0, localTimeDelta*4.0);
    vec3 dir = round(vec3(cos(bikeBAngle), 0.0, -sin(bikeBAngle)));
    vec2 perp = dir.zx * vec2(-1.0, 1.0);
    float rand = fract(sin(iTime * 123.4567)* 123.4567) * 2.0 - 1.0;
    vec2 randPerp = perp * sign(rand);
    ivec2 tpos = ivec2(floor(bikeBPos.xz+(0.5+1.0)*dir.xz)+gameGridCenter);
    vec4 tex = texelFetch(iChannel0, tpos, 0);
    tpos = ivec2(floor(bikeBPos.xz+(0.5+1.0)*dir.xz + randPerp)+gameGridCenter);
    vec4 texPerp = texelFetch(iChannel0, tpos, 0);
    if (animA.x == 256.0) {
        // Bike AI - super advanced, of course.
        // Avoid walls
        if (floor(tex.x) >= floor(bikeBPos.y)) {
            if (floor(texPerp.x) >= floor(bikeBPos.y)) {
                if (rotQueueB == vec2(0)) {
                    if (sign(rand) < 0.0) rotQueueB.x = 1.0;
                    else rotQueueB.x = -1.0;
                }
            }
        }
        // Randomly turn
        if ((abs(rand) < 0.002) && (floor(texPerp.x) < floor(bikeBPos.y))) {
            if (rotQueueB == vec2(0)) {
                if (sign(rand) > 0.0) rotQueueB.x = 1.0;
                else rotQueueB.x = -1.0;
            }
        }

        // Move the bike
        vec3 oldPos = bikeBPos;
        bikeBPos += dir * cycleBSpeed;
        if (floor(bikeBPos+0.5) != floor(oldPos+0.5)) {
            if (rotQueueB != vec2(0.0)) {
                // Turn the bike exactly at the grid crossing
                vec3 newPos = floor(bikeBPos)+vec3(0.5, 0.0, 0.5);
                float remainder = length(bikeBPos - newPos);
                bikeBPos = newPos;
                bikeBAngle += rotQueueB.x * 3.141592653*0.5;
                // Cycle the input queue
                rotQueueB.x = 0.0;
                rotQueueB = rotQueueB.yx;
                // Add the remaining movement vector, but in the new direction
                dir = round(vec3(cos(bikeBAngle), 0.0, -sin(bikeBAngle)));
                bikeBPos += dir * remainder;
            }

            if (animA.x == 256.0) {
                // This must happen after the turn, or we will crash into walls we turned away from
                tpos = ivec2(floor(bikeBPos.xz+0.5*dir.xz)+gameGridCenter);
                tex = texelFetch(iChannel0, tpos, 0);
                // If not a powerup
                if (tex.y < 256.0) {
                    // Bad guy death by wall
                    if (floor(tex.x) >= floor(bikeBPos.y)) {
                        animA.x = 1.0;
                        indicatorPulse = vec4(bikeBPos, 3.0);
                        if (tex.y == 0.0) {
                            gameStage = 100.0;
                            message = 36.0; // ESCAPE!
                        } else {
                            message = 98.0; // you did not escape. try again.
                        }
                    }
                }
            }
        }
    }
    // Death animation
    if ((animA.x >= 0.0) && (animA.x <= 1.0)) {
        animA.x -= localTimeDelta;
        bikeBPos.y -= (1.0 - animA.x) * 0.25;
        bikeBPos += dir * cycleBSpeed;
    }

    tpos = ivec2(bikeBPos.xz+gameGridCenter);
    tex = texelFetch(iChannel0, tpos, 0);
    float inGameGrid = max(abs(bikeBPos.x), abs(bikeBPos.z));

    if (animA.x == 256.0) {
        // If the bike is in the gamegrid and the pixel in in the gamegrid part of the buffer
        if ((floor(bikeBPos.xz) == floor(fragCoord - gameGridCenter)) &&
            (inGameGrid < gameGridRadius)) {
            finalOut.x = round(bikeBPos.y);
            int trail = int(tex.y);
            if (abs(dir.x) > abs(dir.z)) {  // If driving down the x axis
                if (fract(bikeBPos.x) < 0.5) trail |= 16 + 1;
                else trail |= 16 + 2;
            } else {
                if (fract(bikeBPos.z) < 0.5) trail |= 16 + 4;
                else trail |= 16 + 8;
            }
            finalOut.y = float(trail);
        }
        // pick up power ups
        if (tex.y == 256.0) {
            bitsCollected += 1.0;
            if (ivec2(fragCoord) == tpos) {
                finalOut.xy = vec2(0.0, 0.0);
            }
        }
    } else {
        // Erase blue trail completely after death anim
        if (animA.x <= 0.0) {
            if ((finalOut.y >= 16.0) && (finalOut.y < 32.0)) {
                finalOut.xy = vec2(0, 0);
            }
        } else {
            // Destroy the wall where the bad guy bike hit
            if (gameStage >= 100.0) {
                if (distance(floor(bikeBPos.xz + dir.xz), floor(fragCoord - gameGridCenter)) < 4.0) {
                    finalOut.x = 0.0;//round(bikeBPos.y - 1.0);
                }
            }
        }
    }
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // Early out in the hopes of doing less work for higher framerate.
    if ((fragCoord.x > smallestResolution.x + 2.0) || (fragCoord.y > smallestResolution.y + 4.0)) {
        discard;
        return;
    }
    localTimeDelta = min(0.1, iTimeDelta);
    float temp = 3.0;
    fragColor = vec4(0.0);
    LoadState();

    vec2 uv = fragCoord.xy/iResolution.xy;
	vec4 old = texture(iChannel0, uv, -100.0);
    vec4 finalOut = old;
    bool gameDidntStart = ((iMouse.z <= 0.0) && (SampleKey(kKeySpace) <= 0.0)) && (gameStage < 0.0);
    if ((iTime <= 1.0) || (iFrame < 4) || (gameStage >= 1004.0) || gameDidntStart || (iResolution.y < 280.0)) ResetAll(fragCoord, finalOut);
    else if (gameStage < 600.0) {
        if (gameStage <= 0.0) {
            // start game
            gameStage = 0.0;
            message = 13.0;
        }
		MoveBikeA(finalOut, fragCoord);
		MoveBikeB(finalOut, fragCoord);
    }

    // Death animation
    if ((gameStage >= 1000.0) && (gameStage <= 1004.0)) {
        float alpha = saturate(gameStage-1000.0);
        bikeAPos.y -= alpha * 0.25;
	    vec3 dir = round(vec3(cos(bikeAAngle), 0.0, -sin(bikeAAngle)));
	    float cycleASpeed = min(1.0, localTimeDelta*4.0);//4
        bikeAPos += dir * cycleASpeed * (1.0-alpha);
    }

    if ((gameStage >= 503.0) && (gameStage < 600.0)) {
        gameStage = 600.0; // real world
    }
    if ((gameStage >= 608.0) && (gameStage < 700.0)) message = 108.0;

    framerate = mix(framerate, iTimeDelta * 60.0, 0.01);
    message -= localTimeDelta;
    if (mod(message, 10.0) > 8.0) message = 0.0;
    indicatorPulse.w = max(0.0, indicatorPulse.w - 0.015625);

    // Increment gameStage var for animation. Count from start to start + 90.
    if (mod(gameStage, 100.0) < 90.0) gameStage += localTimeDelta;


	finalOut.w = TextRenderer(fragColor, fragCoord);
    //PrintNum(finalOut.w, fragCoord, SampleKey(kKeySpace), ivec2(0, 0));
    //PrintNum(finalOut.w, fragCoord, gameStage, ivec2(0, 0));
    //PrintNum(finalOut.w, fragCoord, iMouse.z, ivec2(0, 5));
    //PrintNum(finalOut.w, fragCoord, bikeAPos.y, ivec2(9, 7));

    StoreState(fragColor, fragCoord);
    // Thumbnail is 288x162 res - actually 256x144 is smallest. :(
    // 500x281 is smallest editor view framebuffer size.
    if ((fragCoord.x < 256.0) && (fragCoord.y < 160.0)) fragColor = finalOut;
    //fragColor = vec4(k1 * 1000.0, sign(uv.x - kKeyLeft),1.0,1.0);
}

