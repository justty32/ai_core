(declare-project :name "greet" :description "native C 模組示範")

# declare-native：把 C 檔編成可 (import greet) 的原生模組
(declare-native
  :name "greet"
  :source ["src/greet.c"])
