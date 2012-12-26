--
-- A few PIC32Lua test functions
-- ASkr 2010; www.askrprojects.net
--
-- V0.1a
--


-- browse available pic functions & variables
function bp()
	local i,j
	
	for i,j in pairs(pic) do
		print(i,j)
	end
end


-- simple port pin toggle
function pt()
	local i

	pic.PortDir(1,1,1)
	
	for i=1,1000 do
		pic.PortState(1,1,1)
		pic.PortState(1,0,1)
	end
end


-- more port pin stuff
function mp()
	local i,j
	
	-- PORTD 0-3 out
	pic.PortDir(3,7,7)
	
	for i=1,100 do
		pic.PinsHigh(3,7)
		pic.PinsLow(3,7)
		
		for j=1,10 do
			pic.PinsInv(3,7)
		end

	end
end


-- read timer
-- ESC quits
function rt()
	local i

	while(1) do
		print(pic.TimerRead())
	end
	
	if pic.ConsoleReadChar()==27 then
		break
	end

end


-- timer match (BLOCKING)
-- ESC quits
function tm()
	local stim
	local tim,timh
	local i

	tim=pic.TimerRead()
	while(1) do
		-- 500ms
		tim=tim+(0.5/100e-9)
		print(tim)
		pic.TimerMatch(tim)
		
		if pic.ConsoleReadChar()==27 then
			break
		end
		
	end
end


-- timer match 2 (NONBLOCKING)
-- ESC quits
function tm2()
	local stim
	local tim,timh
	local i

	tim=pic.TimerRead()
	while(1) do
		-- 500ms
		tim=tim+(0.5/100e-9)
		print(tim)
		
		while pic.TimerMatch(tim,0)==0 do
			-- do something, in here...
		end
		
		if pic.ConsoleReadChar()==27 then
			break
		end
		
	end
end


-- timer match 3 (NONBLOCKING)
-- ESC quits
function tm3()
	local stim
	local tim,timh
	local i

	tim=pic.TimerRead()
	while(1) do
		-- 500ms
		tim=tim+(0.5/100e-9)
		print(tim)
		
		i=0
		while(1) do
			i=i+1
			if pic.TimerMatch(tim,0)==1 then
				break
			end
		end
		
		print("i: "..i)
		
		if pic.ConsoleReadChar()==27 then
			break
		end
		
	end
end


-- console read
-- ESC quits
function cr()
	local ch
	
	repeat
		ch=pic.ConsoleReadChar()
		
		if ch~=nil then
			print(ch)
		end
	
	until ch==27
	
end


-- console count
-- quits after receiving 10 characters
function cc()
	local i
	
	repeat
		i=pic.ConsoleCount()
		print(i)
	until i>10

	pic.ConsoleFlushIn()
	
end


-- UART1 loopback
-- connect UART1 TX and RX together
function ul()
	local i

	pic.UART1Enable(115200)
	
	print("filling RX buffer:")
	for i=65,65+9 do
		print("  sending "..string.char(i))
		pic.UART1WriteChar(i)
	end

	print("check reception:")
	print("buffer contains:"..pic.UART1Count().." characters")
	while(1) do
		i=pic.UART1ReadChar()
		if i ~= nil then
			print("  received "..string.char(i))
		else
			break
		end
	end

	pic.UART1Disable()
	
end


-- UART1 overflow
-- connect UART1 TX and RX together
function uo()
	local i

	pic.UART1Enable(115200)
	
	print("filling RX buffer:")
	for i=1,260 do
		-- reports -1 on overflow
		pic.UART1WriteChar(i)
		print("buffer: "..pic.UART1Count())
	end

	print("flushing buffer...")
	pic.UART1FlushIn()
	print("buffer: "..pic.UART1Count())

	pic.UART1Disable()
	
end

