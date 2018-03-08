from StringDict import strdict

# you can use strdict() in a similar fashion to how dict() is used.
sd = strdict(apples = 100, oranges = "not apples")
print(sd)

for fruit, price in zip(('cherries', 'plums', 'peaches'), (1.25, 2.50, 0.75)):
	sd[fruit] = price

sd["self"] = sd

# for fun
sd.update(locals())

del sd['cherries']
print(sd)
