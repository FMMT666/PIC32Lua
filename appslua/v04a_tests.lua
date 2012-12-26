
-- Just a stupid "SOS" LED toggle for the
-- chipKIT-Max32.
function cksos()
	local i,n
	local stim
	
	pic.PortDir(pic.port.A,0b1000,0b1000)
	pic.PortState(pic.port.A,0,0b1000)
	
	n = 0.15
	stim=pic.TimerRead()
	
	while(1) do
		
		for i=1,6 do

			stim=stim+n/pic.TIMERTICK
			pic.TimerMatch(stim)
		
			pic.PortPinsInv(pic.port.A,0b1000)
		
		end

		if n==0.15 then
			n=0.3
		else
			n=0.15
		end
		
		if pic.ConsoleReadChar() == 27 then
			break
		end

	end
end
