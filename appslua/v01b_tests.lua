--
-- A few PIC32Lua test functions
-- ASkr 2010; www.askrprojects.net
--
-- V0.1b
--



-- binary number notation, timer, console
-- toggles PORTD 0-2; leaves on ESC from console
-- Can be used with the LEDs on a PIC32 starter kit.
function bn()
	local LEDTAB={0b111, 0b000, 0b001, 0b010, 0b100, 0b010, 0b001, 0b000}
	local i
	local stim

	-- set PORTD 0-2 to low
	pic.PortPinsLow(pic.port.D,0b111)
	-- set PORTD 0-2 to output
	pic.PortDir(pic.port.D,0b111,0b111)

	-- read timer value
	stim=pic.TimerRead()

	while 1 do
		for i=1,table.getn(LEDTAB) do
			stim=stim+0.1/pic.TIMERTICK
			pic.PortState(pic.port.D,LEDTAB[i],0b111)
			pic.TimerMatch(stim)
			-- quit on ESC from console
			if pic.ConsoleReadChar()==27 then
				return
			end
			
			print(collectgarbage("count"))
			
		end -- for
	end -- while
	
end



-- end of memory test
-- will fill up the memory until the interpreter stops
function em()
	-- stop the garbage collector
	collectgarbage("stop")
	while(1) do
		print(collectgarbage("count"))
	end
end



-- LED control with upvalues (aka.: external local variables)
-- just a little demonstration of Lua's features
-- Do not call this directly.
-- See "golc()" below...
function lc(port,pin)
	return	function(state)
						if state==0 then
							uvState=0
							pic.PortPinsLow(port,pin)
						else
							uvState=1
							pic.PortPinsHigh(port,pin)
						end
					end
end


-- using upvalues
function golc()
	local stim

	-- clear lower 3 bits of PORTD
	pic.PortPinsLow(pic.port.D,0b111)
	-- set lower 3 bits of PORTD to output
	pic.PortDir(pic.port.D,0b111,0b111)

	-- create 3 LED closures
	led1=lc(pic.port.D,0b001)
	led2=lc(pic.port.D,0b010)
	led3=lc(pic.port.D,0b100)

	-- read timer value
	stim=pic.TimerRead()

	while 1 do
		stim=stim+0.1/pic.TIMERTICK
	
		-- use our new closures...
		led1(math.random(2)-1)
		led2(math.random(2)-1)
		led3(math.random(2)-1)

		pic.TimerMatch(stim)

		-- quit on ESC from console
		if pic.ConsoleReadChar()==27 then
			return
		end
	end
	
end


