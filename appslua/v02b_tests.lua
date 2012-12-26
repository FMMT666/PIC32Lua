
-- AD converter; read values from AN0 (B0)
-- Use precise 200ms timing via TimerMatch()
-- ESC from console quits
function ad()
	local i
	local stim
		
	-- pin to input
	pic.PortDir(pic.port.B,0,0b1)
	-- pin to analog
	pic.ADPins(0b1)
	-- select Vdd and Vss as reference
	pic.ADRef(0b000)
	-- select AN0 as input for MUX A+
	-- 2nd '0' (MUX B+) has no effect
	pic.ADMux(0,0)
	
	stim=pic.TimerRead()
	while 1 do
		stim=stim+0.2/pic.TIMERTICK
		pic.TimerMatch(stim)
		i=pic.ADRead()
		
		-- separate print() to eliminate jitter
		-- (not really required ;-)
		print(i)
	
		if pic.ConsoleReadChar() == 27 then
			break
		end
	end -- while
end


-- "Multi AD" on AN0-3
-- Write RAW values to SD-card.
-- Precise 250ms timing via TimerMatch()
-- ESC from console wuits
-- All in one function; no Lua tricks; only a demonstration...
function mad()
	local ad={}
	local i
	local stim
	local fout

	if pic.CardMount() == 0 then
		print("unable to mount card")
		return -1
	end
	
	fout=io.open("mess.txt","w")
	
	if fout == nil then
		print("unable to open file")
		return -2
	end
	
	-- pins to input
	pic.PortDir(pic.port.B,0,0b1)
	-- pins to analog
	pic.ADPins(0b1)
	-- select Vdd and Vss as reference
	pic.ADRef(0b000)

	stim=pic.TimerRead()
	while 1 do
		local stmp

	-- wait until time matches n*250ms
		stim=stim+0.25/pic.TIMERTICK
		pic.TimerMatch(stim)

		-- read all four channels
		-- from 1 to avoid hashed tables (index 0)
		for i=1,4 do
			pic.ADMux(i-1,0)
			ad[i]=pic.ADRead()
		end

		-- create output string
		stmp=""
		for i=1,4 do
			stmp=stmp..string.format("%4d ",ad[i])
		end
		stmp=stmp.."\r\n"
		
		-- write to SD-card
		fout:write(stmp)		
		
		-- write to console
		io.write(stmp)

		-- quit?
		if pic.ConsoleReadChar() == 27 then
			break
		end
		
	end -- while

	fout:close()

	return 0
	
end

