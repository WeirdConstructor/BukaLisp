from timeit import default_timer as timer

start = timer()
# ...
x = 100000000
while (x > 0):
    x -= 1
end = timer()
print(1000 * (end - start), "ms")
