

func(this *User) Marshal_V1()([]byte,error) {
	returen proto.Marshal(this)
}

func (this *User)Unmarshal_V1()(*User,error){
	return nil,nil
}


