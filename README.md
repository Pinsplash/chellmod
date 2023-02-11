# Leaked code information
This project requires leaked Portal source code to build. No illegally-acquired code is contained in this repo.

# What is this
This is the beautifully named Portal Chell Mod, a mod that houses a rules-based AI for solving Portal test chambers.
## Why did you make it?
One day I searched on the Portal 2 workshop for maps named "impossible". Some of them truly were impossible. It's silly how you can upload a straight-up impossible map, isn't it? And yeah, I'm pretty sure that's where the idea came from. The idea to algorithmically determine if a test was solveable. Either it was that, or playing through The Turing Test and being exposed to the idea of an AI not being able to solve a test. Ever since then I was cursed with the unshakeable thought and truth that it was in fact necessarily possible, somehow. Also, in May 2021 I was a small youtuber who saw a good opportunity.
## Why are you releasing it?
It's cool and I think it would be cool if it were one day finished. I don't plan on finishing it as I've lost motivation.
## How good is it?
It can only solve up to test chamber 2. Some work for 3 is done.
## Could I make this solve my own map?
Provided you do a very minor amount of tweaking to the map, yes.
## Why did you program this insanely?
This was my first programming project in several years. Also, I never formally learned C++. Read it a lot, but didn't write it.
## Can I take this and finish it or do whatever else with it?
Yes.
## Please fix the problem in my error report
No I don't wanna.

# A breakdown of every test chamber that was solved
## Making an NPC that controls a player
https://www.youtube.com/watch?v=44DufMy_FvM

Okay, so how the FUCK is an AI supposed to do anything? Well first, the AI is located in an entity named npc_chell. This is the exact same kind of NPC as a combine soldier or a citizen. This NPC attaches itself to the player upon creation. Then it's told to go somewhere with an input. To go to places, or do any other player action, it makes the player execute console commands. Every player action in Source has a console command associated with it, even looking around. Want the player to move forward? Just use "`+forward`". It's that simple. Well, I mean, THAT part is simple. The actual problem is figuring out what command you want to send at what time.

"Wait, couldn't you have just used a nextbot?"

Probably, but I felt more confident in my ability to understand `CAI_BaseNPC` code.

Most commands are straightforward to implement. Jumping is just "`+jump;wait 10;-jump`". Similar for `+use`. Looking around, however, is not so. Once you establish where you want to look, you have to find if the player is looking above or below that place, and to the left or right of it. To make the player look up, down, left, or right, you have to send `+up`, `+down`, `+left`, or `+right`. Most people don't seem to know, but `cl_mouselook` controls if `+up` and `+down` do anything. We can just change that as well so that those commands work. The console variables `cl_yawspeed` and `cl_pitchspeed` control how quickly the aforementioned commands make the player's camera rotate.

In the video above, `cl_yawspeed` and `cl_pitchspeed` were set to some static numbers, but in later videos, they are also being constantly changed by npc_chell to give much smoother mouse movement. The value that the variables get set to is the difference in angle divided by some amount that's decided every think. It's not just a static number because some mouse movement speeds are quite simply nicer at certain times. Picking up a cube for instance takes longer than necessary when using regular mouse speeds.

Walking is the other major hurdle. For those not familiar with Source NPCs, NPCs need help from the map file to navigate through a map that has walls. This help comes in the form of nodes: little markers of sorts placed on the ground to tell NPCs that they are able to stand in that spot. Nodes have a bunch of testing done on them to determine which nodes can be traveled between. When an NPC wishes to walk from one place to another, it looks at the nodes in the map to construct an ordered list of points that it has to walk to, one after the other. Each point in the list is called a waypoint. When npc_chell wants to walk, it looks at the waypoint that it's supposed to go to next and turns to face it while also using `+forward`, `+back`, `+moveleft`, `+moveright`, or a combination of them, to move in the direction that's closest to the direction that the waypoint is in.

Since our NPC is attached to the player (the technical term being "parented to"), there's a tiny problem: Valve wrote the base NPC code using the "local" variants of the "get position" (`GetLocalOrigin()`) and "get angle" (`GetLocalAngles()`) functions. Those return the position of the NPC relative to whatever it's parented to, if anything. Chell is parented to the player (as how else would it do pathfinding correctly?) and is forcibly teleported to the exact same coordinates as the player, so every single "get position" call would just return `(0, 0, 0)`, so the pathfinding code would act like the npc was at `(0, 0, 0)`. I had to change all of those. Not actually a big change but it felt like something would surely go wrong.

## Walking through portals
https://www.youtube.com/watch?v=TIEfzw1Um5Q

To make an NPC understand that portals can help it move, you have to alter the node graph ("node graph" means the entire collection of nodes in the map). The AI constantly checks which nodes are closest to the portals. If both portals are open, the AI creates an artifical link between the two nodes nearest to the portals. The stock pathfinding algorithm doesn't like the resulting link. It thinks it's too long to be efficient or something. I don't fully understand the ways I tinkered with it, but it worked nonetheless.

There is also a concept in the pathfinding algorithm called zones. Nodes all have a "zone" number that they remember. If two nodes are connected, then they will be given the same zone number. If there is no possible path between two nodes, then they will always have different zones. Zones allowed Valve to quickly reject a pathfind attempt if the node at the start of the path was not in the same zone as the node closest to the goal position. For this mod, however, the zone check had to be forgone, because portals are magic gateways to new zones. This happens a lot: Valve makes a perfectly reasonable assumption about NPC movement, and I have to tear it down.

Then there's the path-simplifying code: a little thing that is dedicated to making NPCs take a more optimized path after having constructed the list of waypoints. The initial list of waypoints is uually not well-crafted and contains jagged paths and stuff. Well, this thing looks at our funky little portal-based path and is like "wow, somebody really messed up" with the result often being that it tells the AI to just go straight to the portal that it's trying to come out of, which will never work because the entire point of a portal is to make movements that otherwise are not possible. So we put a flag on the waypoints near the portals to tell them that they are not allowed to be simplified out of the path, no matter what.

And now we can actually cross through the portal. If we've reached the node closest to portal A and our next waypoint is on the node for portal B, move towards portal A instead of the waypoint. Magic. Oh, also the NPC code might freak out cause it thinks chell is stuck in a wall, but we just gently tell it, "no".

## Test chamber 00
https://www.youtube.com/watch?v=yB8x-dAcjDg

As much as I wanted to have the AI automatically figure out which elevator to go to, it got too complicated to be worth it, so it's told manually to go to the elevator model. Of course, we start off in the vault, cross through the portal, and go to the next room. You can't tell, but at the moment chell started moving, she already knew about the door blocking her access to the elevator (I can't remember the precise reaction this causes as it's been too long to please forgive me), but she immediately forgets about the door due to a hack I had to add. When the player touches the trigger to close the door behind them, chell is sent a "Wait" input that forces her to freeze for 5 seconds. This has to do with what comes next, so let's just explain that now.

Chell doesn't just wait to run into obstacles like other NPCs sometimes do. Chell doesn't even really have those kinds of obstacles. Chell's only actual obstacle here is the door standing between her and the elevator. Chell knows this door is in the way because she's traced invisible lines between each waypoint she's planning to go to and found one that was obstructed. Chell recognizes the door is a special kind of obstacle, one that can be cleared in a special way. One such way is to find a button that's been set up to open the door. Specifically, a trigger entity that has an output for sending the "`Open`" input to the door. If chell finds such a trigger, and she can navigate to it, she starts looking for a cube. If chell finds a cube she can navigate to, then she starts doing that.

This is why chell had to freeze for 5 seconds. If she started moving to that cube as it was falling down, my rudimentary-ass code tried to make her move into the air and I swear to god, NOTHING I did would make her understand that she's not supposed to walk into the air. Anyway, we walk to the cube, look at it, pick it up, then go to the button. Except we don't actually go to the button. We compute a path to the button, then make an `info_target` situated between the last and 2nd to last waypoints in the path. This is the actual place chell walks to. Once at the `info_target`, we look in the direction of the button, and drop the cube. Rarely, chell will accidentally slide too close or too far away from the button for the cube to land right on the button, so we move forward and back as necessary. We also do not drop the cube while moving, as that usually causes it to miss its target due to inertia.

## Test chamber 01
https://www.youtube.com/watch?v=6fy9hZ_QBEU

This chamber is unique both technically and in design. I don't like how non-generalized my approach to it was. It starts by essentially looking through a bunch of the map's logic entities to see if we're dealing with a chamber that has portals that move without originating from a portal gun. If we are, we move into the trigger in the center room that causes the portals to open. (There's no actual "cue" of any sort for that, by the way. A human player decides to go into the center room based entirely on curiosity.) The chamber now proceeds similar to the last one. Pick up cube, put it on button, open door. Only real difference is that after picking up a cube or putting it on a button, we have to go back to the center room. I did it essentially just like that. The AI just goes back to an `info_target` in the center room at the right times.

The real devil of this chamber was in the details. The average first timer will move too slowly to complete the chamber in as few portal changes as possible. They'll likely get stuck in one of the side rooms until the portal comes around again. Chell did too, until I told her how to jump. Jumping in Portal gives you a small horizontal speed boost. When Chell jumps just a bit, she can make it through the chamber in a timely manner. Question is, when should we jump? I decided that chell only needs to jump when it seems to be mathematically necessary, meaning when the portal is going to close in less time than it will take for her to reach it if she were to not jump. The math is conservative, as there are times when chell cannot jump. Chell should not jump for speed if not facing reasonably close to the direction of motion, as the speed boost only applies when moving forward or backward and would still only boost forward/backward even when holding two movement keys. It's also necessary because if chell jumps too far off course, the time gained is actually lost by taking a shit path. Chell also should not jump when about to cross portals because she'll just hit her head. Chell also can't jump when too close to the cube because she'll land on top of it, and since my idiot code detects that she's reached the cube by her colliding with it, she freaks out. The math also has to consider that picking up and dropping cubes takes time.

You probably forgot by now but there was also backwards movement. When carrying a cube through a portal, it's alarmingly common for the cube to get stuck on the rim of the portal, which messed up everything. The easiest solution? Just walk backwards through the portals. Just do it. The math mentioned earlier also had to consider that if walking backwards through a portal with a cube, the cube would get separated from chell if the portal closed after chell crossed but before the cube did.

## Test chamber 02
https://www.youtube.com/watch?v=gCZzVB9QzlA

Okay, remember zones? The numbers that let us instantly know if you can go from one given node to another? Very fortunately, this idea is perfect for our uses, except for one thing: two areas can be connected by jumping paths, and for us, this is bad as Chell is not a good jumper. So, I created a "subzone" system, which works a lot like the zone system, but it ignores jumping connections. If two nodes don't have the same subzone, either we will have to jump downward, or use portals somehow. The possibility of reaching one subzone from another can also be thought of in the same way as the links that exist between nodes, and subzones themselves can be thought of as nodes, and can be pathfinded on using a similar algorithm, though I haven't had to flesh that algorithm out yet.

When first coming into the chamber, the AI tries to make a direct walking path to the elevator. This simple navigation test has been sufficing, but now, we need a more complex nav test. It notices that this won't work due to the test chamber's layout. It has now realized that portals are needed, so it checks to see if it has a portal gun. It does not, so it tries to find a way to the unheld gun. This is the start of the "portal change nav test". The first step to that is finding an orange portal that, if we were next to, would allow us to walk to the gun. If one is found, then we send lines out to the north, south, east, and west of the gun to find where it might place a blue portal. If any of those hypothetical portal places can be reached, we consider that a successful nav test.

Now it goes to the same nav test function again. An important feature of the `PortalChangeNavTest()` function is that it can be told what portal abilities it has - if it has any portal gun at all, if it only has the blue one, or if it has full portal control. At this point in the complex nav test, we know we can reach the blue portal gun, so we send another portal change nav test from the gun to the elevator, but this time we tell the function that we have the blue portal gun. This time inside the test, the AI checks to see if there's a walking path between the orange portal and the exit. While this chamber only has one orange portal, there is also test chamber 03, which is in the same map file, and we don't want to accidentally try to work with that portal, so this check is important. If that check passes, now we have to iterate on every node that's in the subzone that the portal gun is in. Basically, it does a bunch of line traces to find somewhere near a node to place a portal. If any portalable spot is found, the portal change nav test is passed, as well as the complex nav test.

Finally, actual execution can start! Remember before when I mentioned hypothetical portals? We don't actually know if the portal that we want to use will be in the needed spot at any given time, so we need to move there ahead of time. If we simply start moving when it appears, we may not get there quick enough to actually go through it, and it would just waste time. Easier done than said, honestly. Once the player has the gun, we have to fire a shot. We already know where to fire the shot from earlier! We simply have to look to where the portal should be and left click. The rest of the chamber is simple walking.

## Test chamber 03
Work for this chamber is partially complete. Remember the test from the last chamber, where we iterated on every node in a subzone to look for nearby portalable walls? Well, that only works well if that's a subzone you're leaving. That test was not made to consider entering the given subzone. Consider the start of chamber 15. There is no way a test querying that few portal placements could catch every possibility. So I wrote code to trace for portals on every brush face. It's slow. It's buggy. It's your problem now.

# SOURCE 1 SDK LICENSE

Source SDK Copyright(c) Valve Corp.  

THIS DOCUMENT DESCRIBES A CONTRACT BETWEEN YOU AND VALVE 
CORPORATION ("Valve").  PLEASE READ IT BEFORE DOWNLOADING OR USING 
THE SOURCE ENGINE SDK ("SDK"). BY DOWNLOADING AND/OR USING THE 
SOURCE ENGINE SDK YOU ACCEPT THIS LICENSE. IF YOU DO NOT AGREE TO 
THE TERMS OF THIS LICENSE PLEASE DON’T DOWNLOAD OR USE THE SDK.  

  You may, free of charge, download and use the SDK to develop a modified Valve game 
running on the Source engine.  You may distribute your modified Valve game in source and 
object code form, but only for free. Terms of use for Valve games are found in the Steam 
Subscriber Agreement located here: http://store.steampowered.com/subscriber_agreement/ 

  You may copy, modify, and distribute the SDK and any modifications you make to the 
SDK in source and object code form, but only for free.  Any distribution of this SDK must 
include this LICENSE file and thirdpartylegalnotices.txt.  
 
  Any distribution of the SDK or a substantial portion of the SDK must include the above 
copyright notice and the following: 

    DISCLAIMER OF WARRANTIES.  THE SOURCE SDK AND ANY 
    OTHER MATERIAL DOWNLOADED BY LICENSEE IS PROVIDED 
    "AS IS".  VALVE AND ITS SUPPLIERS DISCLAIM ALL 
    WARRANTIES WITH RESPECT TO THE SDK, EITHER EXPRESS 
    OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED 
    WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, 
    TITLE AND FITNESS FOR A PARTICULAR PURPOSE.  

    LIMITATION OF LIABILITY.  IN NO EVENT SHALL VALVE OR 
    ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, 
    INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER 
    (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF 
    BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF 
    BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) 
    ARISING OUT OF THE USE OF OR INABILITY TO USE THE 
    ENGINE AND/OR THE SDK, EVEN IF VALVE HAS BEEN 
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.  
 
       
If you would like to use the SDK for a commercial purpose, please contact Valve at 
sourceengine@valvesoftware.com.
