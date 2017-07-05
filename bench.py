from timeit import default_timer as timer

start = timer()
# ...
x = 100000000
cnt = 0
while (x > 0):
    x -= 1
    cnt = cnt + 1
end = timer()
print(1000 * (end - start))
