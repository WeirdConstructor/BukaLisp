local x = 10000000
local k = 1
local inco
inco = function (a)
    k = k + a
end
while (k < x) do
    inco(1)
end

print(tostring(os.clock()*  1000) .. " ms")
