local k = 1
for i = 1, 100000000 do
    k = k + 1
end
print(tostring(os.clock()*  1000) .. " ms")
