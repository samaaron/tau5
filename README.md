
                                ╘
                         ─       ╛▒╛
                          ▐╫       ▄█├
                   ─╟╛      █▄      ╪▓▀
         ╓┤┤┤┤┤┤┤┤┤  ╩▌      ██      ▀▓▌
          ▐▒   ╬▒     ╟▓╘    ─▓█      ▓▓├
          ▒╫   ▒╪      ▓█     ▓▓─     ▓▓▄
         ╒▒─  │▒       ▓█     ▓▓     ─▓▓─
         ╬▒   ▄▒ ╒    ╪▓═    ╬▓╬     ▌▓▄
         ╥╒   ╦╥     ╕█╒    ╙▓▐     ▄▓╫
                    ▐╩     ▒▒      ▀▀
                         ╒╪      ▐▄

             ______           ______
            /_  __/___  __  _/ ____/
             / / / __ `/ / / /___ \
            / / / /_/ / /_/ /___/ /
           /_/  \__,_/\__,_/_____/

          Collaborative Live Coding
                for Everyone

# Tau5

This is the ground-up re-development of Sonic Pi's core ideas - with a specific focus on collaboration and web compatibility.

The main technology for Tau5 is a VM called the BEAM which hosts both the Erlang and Elixir programming languages whilst also enabling low-latency comms with C++ native code for MIDI/Ableton Link/Discovery, etc.

## Next Collaboration

Tau5 will enable next-level live-coding collaboration. It will support co-located, distributed and async jamming sessions.

* Co-located Jamming with autodiscovery - Tau5 network-connected jam-sessions will enable multiple participants to autodiscover and connect with each other in order to jam in sync. Support for Ableton Link will also be built-in to enable jamming with any Ableton Link powered system (including Ableton Live, Resolume, TOOLL3 and others)

* Distributed Jamming - Tau5 will enable synchronous world-wide jam sessions using a central server for well-timed coordination of events.

* Async Jamming - Tau5 will feature immutable code versions which will enable sharing, forking and modification of compositions/data riffs/algorithms that maintains and preserves provenance.

## Next Language - Tau5Lang

Ruby will not feature in Tau5. This is because it's not suited for sharing and running arbitrary code due to security issues (as is the case for any programming language that has access to system commands - including Python)

A new language - Tau5Lang - will be developed with syntactical similarity to Sonic Pi's Ruby DSL - but based on top of Lua.

Both Lua and Tau5Lang will be supported as first-class-citizen languages. This means it will be possible and safe to run other people's code as part of your own - relying on Lua's amazing sandboxing for security against nefarious algorithms.

For the Lua implementation we will be using Luerl by Robert Virding. Luerl is a version of Lua written in Erlang running on the BEAM VM. This gives us incredible concurrency opportunities in addition to amazingly low-latency IO performance for handling events.

In addition to Lua and Tau5Lang, multiple next-gen mini-DSLs for syntactically precise descriptions of musical ideas are planned. These will all compile down to Lua and work seamlessly with the timing, event and state systems.

## Next GUI

Tau5 will feature a new GUI based on web-technology. This will enable collaborative jam sessions with other devices that have access to a web browser. It will also drastically speed up the pace of development and enable much more exciting exploration and experimentation than the previous C++/Qt based GUI.

For the web-tech stack we will be using Elixir and Phoenix LiveView.

## Next Visuals

Tau5 will incorporate a variety of existing and wonderful web-tech visual projects. The first two confirmed targets will be Hydra by Olivia Jack and p5.js by the Processing Foundation. You will be able to generate and manipulate visuals directly from Tau5Lang in time with your music.

## Next Audio

Tau5 will feature two independent audio stacks - SuperCollider and WebAudio.

* SuperCollider will enable all of the power and stability that you already enjoy in Sonic Pi - all of Sonic Pi's synths, FX and audio capabilities will be directly available in Tau5 as you would expect them to be.

* WebAudio will enable you to stream your music to connected web browser sessions enabling you make sounds in places SuperCollider can't reach.

## Next IO

Tau5 will feature the same rock-solid well-timed OSC and MIDI implementations from Sonic Pi which are already running on the BEAM.

We will also add the ability to send and receive events directly to all participants in your jam session for a new range of IO possibilities.

## Next Intelligence

Tau5 will feature an MCP server directly connected to its core API - to enable LLM/AI agents to work directly with it. Jam with AI, use it to help craft your next drum loop or tweak your synth params.

## Next Dev Tools

Tau5 will feature built-in development tools. This will include the standard web inspector and dev tools, server and gui logs, Elixir Phoenix Dashboard and server REPL. Additionally there will be dev-focussed MCP tools for manipulating and observing both the GUI and the server.
