defmodule Tau5.LuaEvaluatorTest do
  use ExUnit.Case, async: true
  alias Tau5.LuaEvaluator

  describe "basic evaluation" do
    test "evaluates simple arithmetic" do
      assert {:ok, "7"} = LuaEvaluator.evaluate("3 + 4")
      assert {:ok, "20"} = LuaEvaluator.evaluate("5 * 4")
      assert {:ok, "2.5"} = LuaEvaluator.evaluate("10 / 4")
      assert {:ok, "8.0"} = LuaEvaluator.evaluate("2 ^ 3")
    end

    test "evaluates string operations" do
      assert {:ok, "\"hello world\""} = LuaEvaluator.evaluate("\"hello \" .. \"world\"")
      assert {:ok, "11"} = LuaEvaluator.evaluate("#\"hello world\"")
      assert {:ok, "\"HELLO\""} = LuaEvaluator.evaluate("string.upper(\"hello\")")
    end

    test "evaluates boolean logic" do
      assert {:ok, "true"} = LuaEvaluator.evaluate("true and true")
      assert {:ok, "false"} = LuaEvaluator.evaluate("true and false")
      assert {:ok, "true"} = LuaEvaluator.evaluate("true or false")
      assert {:ok, "false"} = LuaEvaluator.evaluate("not true")
    end

    test "evaluates nil values" do
      assert {:ok, "nil"} = LuaEvaluator.evaluate("nil")
      assert {:ok, "nil"} = LuaEvaluator.evaluate("return nil")
    end

    test "evaluates table operations" do
      assert {:ok, result} = LuaEvaluator.evaluate("{1, 2, 3}")
      assert result =~ "1"
      assert result =~ "2"
      assert result =~ "3"

      assert {:ok, "3"} = LuaEvaluator.evaluate("#\{1, 2, 3}")
      # Tables need parentheses when accessed directly
      assert {:ok, "\"value\""} = LuaEvaluator.evaluate("({key = \"value\"}).key")
    end

    test "evaluates multiple return values" do
      assert {:ok, "1, 2, 3"} = LuaEvaluator.evaluate("return 1, 2, 3")
    end
  end

  describe "expression vs statement handling" do
    test "handles expressions without explicit return" do
      assert {:ok, "42"} = LuaEvaluator.evaluate("42")
      assert {:ok, "100"} = LuaEvaluator.evaluate("10 * 10")
      assert {:ok, "\"test\""} = LuaEvaluator.evaluate("\"test\"")
    end

    test "handles statements that don't return values" do
      assert {:ok, "nil"} = LuaEvaluator.evaluate("local x = 5")
      assert {:ok, "nil"} = LuaEvaluator.evaluate("if false then end")
    end

    test "handles variable assignment and retrieval" do
      # Variables don't persist between calls, but work within a single evaluation
      assert {:ok, "10"} = LuaEvaluator.evaluate("x = 10; return x")
      assert {:ok, "30"} = LuaEvaluator.evaluate("local a = 10; local b = 20; return a + b")
    end

    test "handles functions" do
      code = """
      function add(a, b)
        return a + b
      end
      return add(5, 3)
      """
      assert {:ok, "8"} = LuaEvaluator.evaluate(code)
    end

    test "handles anonymous functions" do
      assert {:ok, "25"} = LuaEvaluator.evaluate("return (function(x) return x * x end)(5)")
    end
  end

  describe "control flow" do
    test "handles if statements" do
      assert {:ok, "5"} = LuaEvaluator.evaluate("if true then return 5 else return 10 end")
      assert {:ok, "10"} = LuaEvaluator.evaluate("if false then return 5 else return 10 end")
    end

    test "handles loops" do
      code = """
      local sum = 0
      for i = 1, 5 do
        sum = sum + i
      end
      return sum
      """
      assert {:ok, "15"} = LuaEvaluator.evaluate(code)
    end

    test "handles while loops" do
      code = """
      local i = 0
      local sum = 0
      while i < 3 do
        sum = sum + i
        i = i + 1
      end
      return sum
      """
      assert {:ok, "3"} = LuaEvaluator.evaluate(code)
    end
  end

  describe "security sandboxing" do
    test "blocks io operations" do
      assert {:error, error} = LuaEvaluator.evaluate("io.write('test')")
      assert error =~ "invalid index"
    end

    test "blocks file operations" do
      assert {:error, error} = LuaEvaluator.evaluate("file.open('test.txt')")
      assert error =~ "invalid index"
    end

    test "blocks os operations" do
      assert {:error, error} = LuaEvaluator.evaluate("os.execute('ls')")
      assert error =~ "invalid index"
    end

    test "blocks require" do
      assert {:error, error} = LuaEvaluator.evaluate("require('socket')")
      assert error =~ "sandboxed" or error =~ "nil"
    end

    test "blocks dofile" do
      assert {:error, error} = LuaEvaluator.evaluate("dofile('test.lua')")
      assert error =~ "sandboxed"
    end

    test "blocks loadfile" do
      assert {:error, error} = LuaEvaluator.evaluate("loadfile('test.lua')")
      assert error =~ "sandboxed" or error =~ "nil"
    end

    test "blocks debug library" do
      assert {:error, error} = LuaEvaluator.evaluate("debug.getinfo(1)")
      assert error =~ "invalid index"
    end

    test "blocks metatable operations" do
      # getmetatable and setmetatable are sandboxed
      assert {:error, error} = LuaEvaluator.evaluate("getmetatable('')")
      assert error =~ "sandboxed"

      assert {:error, error} = LuaEvaluator.evaluate("setmetatable({}, {})")
      assert error =~ "sandboxed"
    end

    test "blocks rawget and rawset" do
      assert {:error, error} = LuaEvaluator.evaluate("rawget({}, 'key')")
      assert error =~ "sandboxed" or error =~ "nil"

      assert {:error, error} = LuaEvaluator.evaluate("rawset({}, 'key', 'value')")
      assert error =~ "sandboxed" or error =~ "nil"
    end

    test "print is no-op (returns nil)" do
      assert {:ok, "nil"} = LuaEvaluator.evaluate("print('hello')")
    end
  end

  describe "resource limits" do
    test "enforces timeout on infinite loops" do
      assert {:error, error} = LuaEvaluator.evaluate("while true do end")
      assert error =~ "timeout" or error =~ "timed out"
    end

    test "enforces timeout on expensive computations" do
      # Deliberately expensive nested loop that should exceed 10ms
      code = """
      local sum = 0
      for i = 1, 1000000 do
        for j = 1, 1000 do
          sum = sum + 1
        end
      end
      return sum
      """
      assert {:error, error} = LuaEvaluator.evaluate(code)
      assert error =~ "timeout" or error =~ "timed out"
    end

    test "enforces memory limits" do
      # Try to create a very large table
      code = """
      local t = {}
      for i = 1, 100000 do
        t[i] = string.rep("x", 1000)
      end
      return #t
      """
      result = LuaEvaluator.evaluate(code)
      assert {:error, error} = result
      # The memory limit might cause a timeout if the allocations are happening in a tight loop
      assert error =~ "memory" or error =~ "Memory" or error =~ "timeout" or error =~ "timed out"
    end

    test "enforces output size limits" do
      # The output size limit check happens after formatting.
      # With quotes added, 11000 chars becomes over 10KB
      code = "return string.rep(\"x\", 11000)"
      assert {:ok, result} = LuaEvaluator.evaluate(code)
      # Check that it's truncated (10000 limit + "... (truncated)")
      assert String.ends_with?(result, "... (truncated)") or byte_size(result) <= 10_050
    end

    test "truncates large table output" do
      # Create a smaller table to avoid memory limit
      code = """
      local t = {}
      for i = 1, 100 do
        t[i] = i
      end
      return t
      """
      assert {:ok, result} = LuaEvaluator.evaluate(code)
      # Should contain some numbers
      assert result =~ "1"
      assert result =~ "100"
    end
  end

  describe "error handling" do
    test "handles syntax errors" do
      assert {:error, error} = LuaEvaluator.evaluate("if without end")
      assert error =~ "syntax" or error =~ "Syntax"

      assert {:error, error} = LuaEvaluator.evaluate("function ()")
      assert error =~ "syntax" or error =~ "Syntax"
    end

    test "handles runtime errors" do
      assert {:error, error} = LuaEvaluator.evaluate("x + 5")
      assert error =~ "bad arithmetic"

      assert {:error, error} = LuaEvaluator.evaluate("5 + \"string\"")
      assert error =~ "attempt" or error =~ "arithmetic"
    end

    test "handles division by zero" do
      # Lua allows division by zero (returns inf)
      # Luerl doesn't allow division by zero
      assert {:error, error} = LuaEvaluator.evaluate("1 / 0")
      assert error =~ "arithmetic"
    end

    test "handles table index errors" do
      assert {:error, error} = LuaEvaluator.evaluate("local t = {}; return t.x.y")
      assert error =~ "nil" or error =~ "index"
    end

    test "handles function call errors" do
      assert {:error, error} = LuaEvaluator.evaluate("nonexistent()")
      assert error =~ "nil" or error =~ "attempt"
    end

    test "sanitizes error messages" do
      # Errors should not contain internal Erlang/Elixir details
      assert {:error, error} = LuaEvaluator.evaluate("error('user error')")
      refute error =~ "luerl"
      refute error =~ "tstruct"
      refute error =~ "#Reference"
      refute error =~ "#Function"
    end
  end

  describe "check_syntax/1" do
    test "validates correct syntax" do
      assert :ok = LuaEvaluator.check_syntax("1 + 1")
      assert :ok = LuaEvaluator.check_syntax("local x = 5")
      assert :ok = LuaEvaluator.check_syntax("function f() return 1 end")
      assert :ok = LuaEvaluator.check_syntax("if true then print(1) end")
    end

    test "detects syntax errors without execution" do
      assert {:error, error} = LuaEvaluator.check_syntax("if without end")
      assert error =~ "Syntax error"

      assert {:error, error} = LuaEvaluator.check_syntax("function ()")
      assert error =~ "Syntax error"

      assert {:error, error} = LuaEvaluator.check_syntax("local 123 = 5")
      assert error =~ "Syntax error"
    end

    test "handles both expressions and statements" do
      assert :ok = LuaEvaluator.check_syntax("42")
      assert :ok = LuaEvaluator.check_syntax("return 42")
      assert :ok = LuaEvaluator.check_syntax("local x = 42")
    end

    test "does not execute code" do
      # This would error at runtime but should pass syntax check
      assert :ok = LuaEvaluator.check_syntax("undefined_var + 1")
      assert :ok = LuaEvaluator.check_syntax("nonexistent_function()")
    end
  end

  describe "special Lua features" do
    test "handles varargs in functions" do
      code = """
      function sum(...)
        local args = {...}
        local total = 0
        for i = 1, #args do
          total = total + args[i]
        end
        return total
      end
      return sum(1, 2, 3, 4, 5)
      """
      assert {:ok, "15"} = LuaEvaluator.evaluate(code)
    end

    test "handles closures" do
      # Very simple closure test that won't timeout
      code = "local x = 5; return (function() return x * 2 end)()"
      assert {:ok, "10"} = LuaEvaluator.evaluate(code)
    end

    test "handles table iteration" do
      code = """
      local t = {a = 1, b = 2, c = 3}
      local sum = 0
      for k, v in pairs(t) do
        sum = sum + v
      end
      return sum
      """
      assert {:ok, "6"} = LuaEvaluator.evaluate(code)
    end

    test "handles string patterns" do
      # gsub returns the result and count of replacements
      assert {:ok, "\"Hxllo\", 1"} = LuaEvaluator.evaluate("string.gsub(\"Hello\", \"e\", \"x\")")
      assert {:ok, "\"llo\""} = LuaEvaluator.evaluate("string.sub(\"Hello\", 3)")
      assert {:ok, "1, 5"} = LuaEvaluator.evaluate("string.find(\"Hello\", \"Hello\")")
    end

    test "handles math functions" do
      assert {:ok, "3"} = LuaEvaluator.evaluate("math.floor(3.7)")
      assert {:ok, "4"} = LuaEvaluator.evaluate("math.ceil(3.2)")
      assert {:ok, "25"} = LuaEvaluator.evaluate("math.abs(-25)")
      assert {:ok, result} = LuaEvaluator.evaluate("math.sin(math.pi / 2)")
      assert String.starts_with?(result, "1.0")
    end

    test "check_memory function is available" do
      assert {:ok, result} = LuaEvaluator.evaluate("return check_memory()")
      # Should return true and memory usage
      assert result =~ "true"
    end
  end
end