
require("test_another")

test_variable = "hello world!"
print(test_variable .. " this is from lua!")
print(test_variable .. " this is another test.")

test_table = {
  test_string = "foo bar",
  test_number = 64
}


local function print_iter(n)
  local test_local = {
    test_str = "test"
  }

  for i = 1, n, 1 do
    test_print(string.format("number: %d", i));
  end
end

print_iter(5)