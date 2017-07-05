
local x = 10000000
local k = 1
local cnt = 0
while (x > 0) do
    x = x - k
    cnt = cnt + k
end

print(tostring(os.clock()))
