--
-- The obligatory speed test...
-- Ok, somehow ridiculous ;-)
--


-- fastes Fibonacci calculation in Lua
function fib2(n)
	local a,b = 0,1
	while n > 0 do
     a,b = b,a+b
     n=n-1
  end
  return a
end

-- 300 Fibonacci calculations in ~1s
-- (without recursion, of course ;-)
function gofib(n)
	local stim
	local i,j

	if n==nil then
		n=300
	end
	
	stim=pic.TimerRead()
	for i=1,n do
		io.write(" "..fib2(i))
--		fib2(i)
	end
	stim=pic.TimerRead()-stim
	io.flush()
	print() print()
	print("CALCULATION TOOK "..stim*pic.TIMERTICK.."s FOR "..n.." NUMBERS")
end

