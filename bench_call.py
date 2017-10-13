from timeit import default_timer as timer

start = timer()
# ...
x = 10000000
def inco(a):
    global x
    x = x + a

while (x > 0):
    inco(-1)

end = timer()
print(1000 * (end - start), "ms")
