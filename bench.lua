
local x = 10000000
local k = 1
--local o = function (a) k = k + a end
while (k < x) do
--    o(1)
    k = k + 1
end

print(tostring(os.clock()))
